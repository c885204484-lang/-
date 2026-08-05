#include "stm32f10x.h"
#include "BlueTooth.h"
#include "PetState.h"
#include "PetAction.h"
#include "ConfigStore.h"
#include "Servo.h"
#include "Variable.h"
#include "Watchdog.h"
#include "Scheduler.h"

#define RX_BUFFER_SIZE 64
#define FRAME_DATA_MAX 16

typedef struct
{
	volatile uint8_t data[RX_BUFFER_SIZE];
	volatile uint8_t head;
	volatile uint8_t tail;
	volatile uint16_t overflow;
} RxBuffer;

static RxBuffer voice_rx;
static RxBuffer bluetooth_rx;
static uint16_t protocol_errors;

static void BufferPush(RxBuffer *buffer, uint8_t value)
{
	uint8_t next = (uint8_t)((buffer->head + 1) % RX_BUFFER_SIZE);
	if (next == buffer->tail) { buffer->overflow++; return; }
	buffer->data[buffer->head] = value;
	buffer->head = next;
}

static uint8_t BufferPop(RxBuffer *buffer, uint8_t *value)
{
	if (buffer->head == buffer->tail) return 0;
	*value = buffer->data[buffer->tail];
	buffer->tail = (uint8_t)((buffer->tail + 1) % RX_BUFFER_SIZE);
	return 1;
}

static void SendByte(uint8_t value)
{
	uint32_t timeout = 100000;
	while (USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET && timeout) timeout--;
	if (timeout) USART_SendData(USART3, value);
}

static void SendFrame(uint8_t command, const uint8_t *data, uint8_t length)
{
	uint8_t i;
	uint8_t checksum = (uint8_t)(0xAA + 0x55 + command + length);
	SendByte(0xAA); SendByte(0x55); SendByte(command); SendByte(length);
	for (i = 0; i < length; i++) { SendByte(data[i]); checksum += data[i]; }
	SendByte(checksum);
}

static void DispatchCommand(uint8_t command, const uint8_t *data, uint8_t length, uint8_t sustained)
{
	uint8_t response = 1;
	if (command >= 0x29 && command <= 0x50) response = PetState_Command(command, sustained);
	else if (command == 0x60) { BlueTooth_SendStatus(); return; }
	else if (command == 0x61) response = ConfigStore_Save();
	else if (command == 0x62) { PetAction_Stop(); response = 1; }
	else if (command == 0x63 && length >= 2)
	{
		uint16_t speed = (uint16_t)data[0] * 10;
		uint8_t swing = data[1];
		if (speed >= 80 && speed <= 400 && swing >= 2 && swing <= 20)
		{
			SpeedDelay = speed; SwingDelay = swing;
		}
		else response = 0;
	}
	else if (command == 0x64 && length >= 4)
	{
		if (data[0] < SERVO_COUNT && data[1] < data[2] && data[2] <= 180 && (int8_t)data[3] >= -30 && (int8_t)data[3] <= 30)
			Servo_SetCalibration(data[0], data[1], data[2], (int8_t)data[3]);
		else response = 0;
	}
	else response = 0;
	if (sustained) SendFrame(0x7F, &response, 1);
}

static void ParseBuffer(RxBuffer *buffer, uint8_t sustained)
{
	static uint8_t state[2], command[2], length[2], index[2], checksum[2];
	static uint8_t payload[2][FRAME_DATA_MAX];
	static uint32_t last_byte_ms[2];
	uint8_t channel = sustained ? 1 : 0;
	uint8_t value;
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
				if (value == 0xAA) { state[channel] = 1; checksum[channel] = value; }
				else DispatchCommand(value, 0, 0, sustained);
				break;
			case 1:
				if (value == 0x55) { state[channel] = 2; checksum[channel] += value; }
				else { state[channel] = 0; protocol_errors++; }
				break;
			case 2: command[channel] = value; checksum[channel] += value; state[channel] = 3; break;
			case 3:
				length[channel] = value; checksum[channel] += value; index[channel] = 0;
				if (value > FRAME_DATA_MAX) { state[channel] = 0; protocol_errors++; }
				else state[channel] = value ? 4 : 5;
				break;
			case 4:
				payload[channel][index[channel]++] = value; checksum[channel] += value;
				if (index[channel] >= length[channel]) state[channel] = 5;
				break;
			case 5:
				if (value == checksum[channel]) DispatchCommand(command[channel], payload[channel], length[channel], sustained);
				else protocol_errors++;
				state[channel] = 0;
				break;
		}
	}
}

void BlueTooth_Init(void)
{
	GPIO_InitTypeDef gpio;
	USART_InitTypeDef usart;
	NVIC_InitTypeDef nvic;
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOB, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);
	gpio.GPIO_Mode = GPIO_Mode_AF_PP; gpio.GPIO_Speed = GPIO_Speed_50MHz;
	gpio.GPIO_Pin = GPIO_Pin_9; GPIO_Init(GPIOA, &gpio);
	gpio.GPIO_Pin = GPIO_Pin_10; GPIO_Init(GPIOB, &gpio);
	gpio.GPIO_Mode = GPIO_Mode_IPU;
	gpio.GPIO_Pin = GPIO_Pin_10; GPIO_Init(GPIOA, &gpio);
	gpio.GPIO_Pin = GPIO_Pin_11; GPIO_Init(GPIOB, &gpio);
	usart.USART_BaudRate = 9600;
	usart.USART_WordLength = USART_WordLength_8b;
	usart.USART_StopBits = USART_StopBits_1;
	usart.USART_Parity = USART_Parity_No;
	usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	usart.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(USART1, &usart);
	usart.USART_BaudRate = 115200; USART_Init(USART3, &usart);
	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
	USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
	nvic.NVIC_IRQChannelCmd = ENABLE;
	nvic.NVIC_IRQChannelPreemptionPriority = 1; nvic.NVIC_IRQChannelSubPriority = 1;
	nvic.NVIC_IRQChannel = USART1_IRQn; NVIC_Init(&nvic);
	nvic.NVIC_IRQChannelPreemptionPriority = 2;
	nvic.NVIC_IRQChannel = USART3_IRQn; NVIC_Init(&nvic);
	USART_Cmd(USART1, ENABLE); USART_Cmd(USART3, ENABLE);
}

void BlueTooth_Task(void)
{
	ParseBuffer(&voice_rx, 0);
	ParseBuffer(&bluetooth_rx, 1);
}

void BlueTooth_SendStatus(void)
{
	const PetStatus *pet = PetState_Get();
	uint8_t data[14];
	uint8_t i;
	data[0] = (uint8_t)CurBattery;
	data[1] = pet->mood; data[2] = pet->energy; data[3] = pet->activity;
	data[4] = (uint8_t)Action_Mode; data[5] = (uint8_t)pet->health; data[6] = pet->sleeping;
	for (i = 0; i < SERVO_COUNT; i++) data[7 + i] = Servo_GetTarget(i);
	data[12] = (uint8_t)protocol_errors;
	data[13] = Watchdog_WasReset();
	SendFrame(0x70, data, sizeof(data));
}

uint16_t BlueTooth_GetErrorCount(void)
{
	return protocol_errors + voice_rx.overflow + bluetooth_rx.overflow;
}

void USART1_IRQHandler(void)
{
	if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET) BufferPush(&voice_rx, (uint8_t)USART_ReceiveData(USART1));
}

void USART3_IRQHandler(void)
{
	if (USART_GetITStatus(USART3, USART_IT_RXNE) != RESET) BufferPush(&bluetooth_rx, (uint8_t)USART_ReceiveData(USART3));
}
