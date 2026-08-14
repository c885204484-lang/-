/**
  ******************************************************************************
  * @file    BlueTooth.c
  * @brief   双通道控制：USART1 语音(9600) + USART3 蓝牙(115200)，环形缓冲 + 协议解析
  *
  * 【架构】
  *   1. 中断(RXNE)只做一件事：把收到的字节压入对应环形缓冲区（极短，不阻塞）；
  *   2. 主循环 BlueTooth_Task() 统一从缓冲区取字节解析，实现"中断入队、主循环出队"；
  *   3. 兼容两种协议：
  *      - 旧版单字节命令：直接按命令字节分发（0x28~0x50）；
  *      - 新版数据帧：AA 55 CMD LEN DATA... CHECKSUM（累加和校验，超100ms丢弃）
  *
  * 【数据帧格式】
  *   AA 55 CMD LEN DATA[0..15] CHECKSUM
  *   - CHECKSUM = (AA+55+CMD+LEN+DATA...) 累加和的低 8 位
  *   - LEN > 16 视为非法帧，丢弃
  *   - 新增命令：0x60 查询状态 / 0x61 保存配置 / 0x62 紧急停止 / 0x63 设速度 / 0x64 舵机校准
  *
  * 【应答】
  *   蓝牙通道(USART3)：0x7F 确认帧（1=成功 0=拒绝），0x70 状态遥测帧
  *   语音通道(USART1)：不主动回复
  ******************************************************************************
  */
#include "stm32f10x.h"
#include "BlueTooth.h"
#include "PetState.h"
#include "PetAction.h"
#include "ConfigStore.h"
#include "Servo.h"
#include "Variable.h"
#include "Watchdog.h"
#include "Scheduler.h"

#define RX_BUFFER_SIZE 64     /* 每个串口的环形缓冲长度 */
#define FRAME_DATA_MAX 16     /* 数据帧 DATA 区最大长度 */

/* 环形缓冲区结构：head 写入端，tail 读取端，满则溢出计数 */
typedef struct
{
	volatile uint8_t data[RX_BUFFER_SIZE];
	volatile uint8_t head;
	volatile uint8_t tail;
	volatile uint16_t overflow;   /* 溢出次数统计（遥测上报） */
} RxBuffer;

static RxBuffer voice_rx;         /* 语音通道(USART1)接收缓冲 */
static RxBuffer bluetooth_rx;     /* 蓝牙通道(USART3)接收缓冲 */
static uint16_t protocol_errors;  /* 协议错误计数（校验失败/超时/长度非法） */

/**
  * @brief  环形缓冲入队（中断中使用）
  * @retval 无；缓冲满时溢出计数 +1 并丢弃该字节
  */
static void BufferPush(RxBuffer *buffer, uint8_t value)
{
	uint8_t next = (uint8_t)((buffer->head + 1) % RX_BUFFER_SIZE);
	if (next == buffer->tail) { buffer->overflow++; return; }   /* 满：丢字节并计数 */
	buffer->data[buffer->head] = value;
	buffer->head = next;
}

/**
  * @brief  环形缓冲出队（主循环中使用）
  * @retval 1=成功取到一个字节；0=缓冲区为空
  */
static uint8_t BufferPop(RxBuffer *buffer, uint8_t *value)
{
	if (buffer->head == buffer->tail) return 0;
	*value = buffer->data[buffer->tail];
	buffer->tail = (uint8_t)((buffer->tail + 1) % RX_BUFFER_SIZE);
	return 1;
}

/**
  * @brief  通过 USART3(蓝牙)发送一个字节（带超时保护，防止 TXE 一直不置位卡死）
  */
static void SendByte(uint8_t value)
{
	uint32_t timeout = 100000;
	while (USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET && timeout) timeout--;
	if (timeout) USART_SendData(USART3, value);
}

/**
  * @brief  通过 USART3 发送一帧数据：AA 55 CMD LEN DATA... CHECKSUM
  * @param  command 命令字节
  * @param  data    数据区指针（可为 NULL）
  * @param  length  数据区长度
  */
static void SendFrame(uint8_t command, const uint8_t *data, uint8_t length)
{
	uint8_t i;
	uint8_t checksum = (uint8_t)(0xAA + 0x55 + command + length);   /* 累加和校验初值 */
	SendByte(0xAA); SendByte(0x55); SendByte(command); SendByte(length);
	for (i = 0; i < length; i++) { SendByte(data[i]); checksum += data[i]; }
	SendByte(checksum);
}

/**
  * @brief  命令分发：根据命令字节执行对应操作
  * @param  command  命令字节
  * @param  data     数据帧的 DATA 区（单字节命令时为 NULL）
  * @param  length   DATA 区长度
  * @param  sustained 是否持续执行（蓝牙通道移动命令）
  */
static void DispatchCommand(uint8_t command, const uint8_t *data, uint8_t length, uint8_t sustained)
{
	uint8_t response = 1;
	if (command >= 0x28 && command <= 0x50) response = PetState_Command(command, sustained);   /* 旧版单字节命令交给状态机 */
	else if (command == 0x60) { Status_Display_Bit = 1; Status_Display_Seconds = 5; BlueTooth_SendStatus(); return; }  /* 查询状态：显示状态页并回遥测帧 */
	else if (command == 0x61) response = ConfigStore_Save();       /* 保存配置到 Flash */
	else if (command == 0x62)                                      /* 紧急停止：回站立、停止摇尾 */
	{
		PetAction_Stop();
		Face_Mode = 5;
		Status_Display_Bit = 0;
		WeiBa_Bit = 0;
		Servo_SetTarget(4, 90, 100);
		response = 1;
	}
	else if (command == 0x63 && length >= 2)                       /* 设置移动速度(速度/10)与摇摆延时 */
	{
		uint16_t speed = (uint16_t)data[0] * 10;
		uint8_t swing = data[1];
		if (speed >= 80 && speed <= 400 && swing >= 2 && swing <= 20)
		{
			SpeedDelay = speed; SwingDelay = swing;                /* 校验通过才写入 */
		}
		else response = 0;
	}
	else if (command == 0x64 && length >= 4)                       /* 设置舵机校准：servo, min, max, trim */
	{
		if (data[0] < SERVO_COUNT && data[1] < data[2] && data[2] <= 180 && (int8_t)data[3] >= -30 && (int8_t)data[3] <= 30)
			Servo_SetCalibration(data[0], data[1], data[2], (int8_t)data[3]);
		else response = 0;
	}
	else response = 0;
	if (sustained) SendFrame(0x7F, &response, 1);   /* 蓝牙通道回复 1 字节确认帧 */
}

/**
  * @brief  解析一个环形缓冲中的数据（核心协议解析状态机）
  * @param  buffer    目标环形缓冲
  * @param  sustained 该通道是否持续执行移动命令（蓝牙=1，语音=0）
  * @note   state[channel] 状态机：
  *         0 等帧头AA -> 1 等0x55 -> 2 收CMD -> 3 收LEN -> 4 收DATA -> 5 校验CHECKSUM
  *         状态1以后若 100ms 内无新字节，视为残帧丢弃（防半包卡死状态机）
  */
static void ParseBuffer(RxBuffer *buffer, uint8_t sustained)
{
	static uint8_t state[2], command[2], length[2], index[2], checksum[2];   /* 双通道各自的解析状态 */
	static uint8_t payload[2][FRAME_DATA_MAX];                               /* 双通道各自的数据区 */
	static uint32_t last_byte_ms[2];                                         /* 双通道各自最后收字节时刻 */
	uint8_t channel = sustained ? 1 : 0;   /* 用 sustained 区分通道：0=语音，1=蓝牙 */
	uint8_t value;
	/* 帧超时保护：正在收帧但超过 100ms 没新字节，丢弃残帧 */
	if (state[channel] && (uint32_t)(Scheduler_Millis() - last_byte_ms[channel]) > 100)
	{
		state[channel] = 0;
		protocol_errors++;
	}
	while (BufferPop(buffer, &value))
	{
		last_byte_ms[channel] = Scheduler_Millis();
		switch (state[channel])
		{
			case 0:
				/* 帧头：0xAA 进入组帧状态；否则视为旧版单字节命令直接分发 */
				if (value == 0xAA) { state[channel] = 1; checksum[channel] = value; }
				else DispatchCommand(value, 0, 0, sustained);
				break;
			case 1:
				/* 第二帧头：必须为 0x55，否则协议错误并回到等待帧头 */
				if (value == 0x55) { state[channel] = 2; checksum[channel] += value; }
				else { state[channel] = 0; protocol_errors++; }
				break;
			case 2: command[channel] = value; checksum[channel] += value; state[channel] = 3; break;   /* 命令字节 */
			case 3:
				/* 长度字节：>16 非法；=0 直接跳到校验；否则进入收数据 */
				length[channel] = value; checksum[channel] += value; index[channel] = 0;
				if (value > FRAME_DATA_MAX) { state[channel] = 0; protocol_errors++; }
				else state[channel] = value ? 4 : 5;
				break;
			case 4:
				/* 数据区：收满 length 个字节后进入校验 */
				payload[channel][index[channel]++] = value; checksum[channel] += value;
				if (index[channel] >= length[channel]) state[channel] = 5;
				break;
			case 5:
				/* 校验字节：与累加和低 8 位一致则执行命令，否则计协议错误 */
				if (value == checksum[channel]) DispatchCommand(command[channel], payload[channel], length[channel], sustained);
				else protocol_errors++;
				state[channel] = 0;
				break;
		}
	}
}

/**
  * @brief  双串口初始化：USART1(语音 9600, PA9=TX PA10=RX) + USART3(蓝牙 115200, PB10=TX PB11=RX)
  * @note   两个串口都只开 RXNE 接收中断；NVIC 分组2，USART1 抢占优先级1，USART3 抢占优先级2
  */
void BlueTooth_Init(void)
{
	GPIO_InitTypeDef gpio;
	USART_InitTypeDef usart;
	NVIC_InitTypeDef nvic;
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOB, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);
	/* TX 引脚：复用推挽输出 */
	gpio.GPIO_Mode = GPIO_Mode_AF_PP; gpio.GPIO_Speed = GPIO_Speed_50MHz;
	gpio.GPIO_Pin = GPIO_Pin_9; GPIO_Init(GPIOA, &gpio);     /* USART1_TX = PA9 */
	gpio.GPIO_Pin = GPIO_Pin_10; GPIO_Init(GPIOB, &gpio);    /* USART3_TX = PB10 */
	/* RX 引脚：上拉输入 */
	gpio.GPIO_Mode = GPIO_Mode_IPU;
	gpio.GPIO_Pin = GPIO_Pin_10; GPIO_Init(GPIOA, &gpio);    /* USART1_RX = PA10 */
	gpio.GPIO_Pin = GPIO_Pin_11; GPIO_Init(GPIOB, &gpio);    /* USART3_RX = PB11 */
	/* USART1：9600bps，8位数据，无校验，1停止位 */
	usart.USART_BaudRate = 9600;
	usart.USART_WordLength = USART_WordLength_8b;
	usart.USART_StopBits = USART_StopBits_1;
	usart.USART_Parity = USART_Parity_No;
	usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	usart.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(USART1, &usart);
	/* USART3：115200bps，其余参数相同 */
	usart.USART_BaudRate = 115200; USART_Init(USART3, &usart);
	/* 开启两个串口的接收中断 */
	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
	USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);
	/* 中断优先级：语音(1) > 蓝牙(2)，确保语音指令优先处理 */
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
	nvic.NVIC_IRQChannelCmd = ENABLE;
	nvic.NVIC_IRQChannelPreemptionPriority = 1; nvic.NVIC_IRQChannelSubPriority = 1;
	nvic.NVIC_IRQChannel = USART1_IRQn; NVIC_Init(&nvic);
	nvic.NVIC_IRQChannelPreemptionPriority = 2;
	nvic.NVIC_IRQChannel = USART3_IRQn; NVIC_Init(&nvic);
	USART_Cmd(USART1, ENABLE); USART_Cmd(USART3, ENABLE);
}

/**
  * @brief  串口周期任务（主循环持续调用）：解析两个通道的缓冲区
  */
void BlueTooth_Task(void)
{
	ParseBuffer(&voice_rx, 0);       /* 语音通道：单次动作 */
	ParseBuffer(&bluetooth_rx, 1);   /* 蓝牙通道：支持持续动作 + 应答帧 */
}

/**
  * @brief  发送状态遥测帧 0x70（数据区固定 14 字节）
  * @note   格式见 README：电量/心情/精力/活跃度/动作/健康/睡眠/5路舵机目标角/协议错误数/看门狗复位标志
  */
void BlueTooth_SendStatus(void)
{
	const PetStatus *pet = PetState_Get();
	uint8_t data[14];
	uint8_t i;
	data[0] = (uint8_t)CurBattery;                              /* 电量 */
	data[1] = pet->mood; data[2] = pet->energy; data[3] = pet->activity;   /* 心情/精力/活跃度 */
	data[4] = (uint8_t)Action_Mode; data[5] = (uint8_t)pet->health; data[6] = pet->sleeping;  /* 动作/健康/睡眠 */
	for (i = 0; i < SERVO_COUNT; i++) data[7 + i] = Servo_GetTarget(i);    /* 5 路舵机目标角度 */
	data[12] = (uint8_t)protocol_errors;                        /* 协议错误数(低8位) */
	data[13] = Watchdog_WasReset();                             /* 看门狗复位标志 */
	SendFrame(0x70, data, sizeof(data));
}

/**
  * @brief  获取总协议错误计数（协议错误 + 两个缓冲的溢出次数）
  */
uint16_t BlueTooth_GetErrorCount(void)
{
	return protocol_errors + voice_rx.overflow + bluetooth_rx.overflow;
}

/**
  * @brief  USART1 接收中断（语音模块）：收到一个字节就入语音缓冲
  * @note   中断内只做入队，解析在主循环 BlueTooth_Task() 完成
  */
void USART1_IRQHandler(void)
{
	if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET) BufferPush(&voice_rx, (uint8_t)USART_ReceiveData(USART1));
}

/**
  * @brief  USART3 接收中断（蓝牙模块）：收到一个字节就入蓝牙缓冲
  */
void USART3_IRQHandler(void)
{
	if (USART_GetITStatus(USART3, USART_IT_RXNE) != RESET) BufferPush(&bluetooth_rx, (uint8_t)USART_ReceiveData(USART3));
}
