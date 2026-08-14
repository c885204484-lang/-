/**
  ******************************************************************************
  * @file    main.c
  * @brief   智能桌面宠物主程序：完成所有外设初始化后，进入非阻塞周期调度主循环
  *
  * 【软件架构】裸机调度器（无 RTOS）
  *   - Scheduler.c 基于 SysTick 提供 1ms 时基（Scheduler_Millis()）
  *   - while(1) 通过 Scheduler_Elapsed() 判断各任务是否到达执行周期
  *   - 串口中断只负责把收到的字节放入环形缓冲区，解析统一在主循环完成，
  *     保证中断服务函数极短，避免阻塞舵机 PWM 等实时任务
  *   - 每轮循环末尾喂狗（Watchdog_Feed）并执行 __WFI() 进入低功耗等待
  *
  * 【主循环任务周期一览】
  *   持续    BlueTooth_Task()       解析语音(USART1)/蓝牙(USART3)命令
  *   20ms    PetAction_Task20ms()   舵机平滑插值 + 动作关键帧推进
  *   100ms   PetState_Task100ms()   尾巴摇动联动；LED 常亮/呼吸/关闭
  *   200ms   Face_Config()          清醒时按需刷新 OLED 表情/电量显示
  *   500ms   GetCur_Power()         读取 ADC 并更新滤波后的电量值
  *   1000ms  PetState_Task1s()      心情/精力/睡眠/健康状态机 + 蓝牙状态遥测
  *
  * 【省电策略】宠物睡眠(sleeping)时：
  *   - OLED 刷新周期从 200ms 延长到 1000ms
  *   - 状态更新与蓝牙遥测周期从 1000ms 延长到 5000ms
  ******************************************************************************
  */
#include "stm32f10x.h"
#include "OLED.h"
#include "BlueTooth.h"
#include "Servo.h"
#include "PetAction.h"
#include "PetState.h"
#include "Face_Config.h"
#include "PowerDetection.h"
#include "Led_Breathing.h"
#include "Scheduler.h"
#include "ConfigStore.h"
#include "Watchdog.h"

/**
  * @brief  程序入口：初始化所有外设后进入主循环
  * @note   初始化顺序有讲究：先加载 Flash 配置（决定舵机限位/速度等），
  *         再初始化依赖这些参数的硬件，最后才启动看门狗，避免初始化期间被复位
  */
int main(void)
{
	uint32_t task20 = 0;    /* 20ms 任务上次执行时刻（调度器用） */
	uint32_t task100 = 0;   /* 100ms 任务上次执行时刻 */
	uint32_t task200 = 0;   /* 200ms 任务上次执行时刻 */
	uint32_t task500 = 0;   /* 500ms 任务上次执行时刻 */
	uint32_t task1000 = 0;  /* 1000ms 任务上次执行时刻 */

	ConfigStore_Load();     /* 从 Flash 读取上次保存的配置（速度/灯光/宠物状态/舵机校准），非法则用默认值 */
	GetBattery_Init();      /* 电量检测初始化 -> AD_Init()：配置 PA4 为模拟输入，ADC1+DMA 循环采样 */
	Servo_Init();           /* 舵机初始化 -> PWM_Init()：TIM2 四通道 + TIM3 通道1 输出 50Hz PWM 驱动 5 路舵机 */
	OLED_Init();            /* OLED 初始化（SSD1306，软件 I2C，PB8=SCL PB9=SDA） */
	BlueTooth_Init();       /* 双串口初始化：USART1(PA9/PA10,9600)接语音模块，USART3(PB10/PB11,115200)接蓝牙 */
	Scheduler_Init();       /* 启动 SysTick 1ms 时基 */
	PetState_Init();        /* 初始化宠物心情/精力/活跃度/睡眠/健康状态（从配置恢复） */
	PetAction_Init();       /* 初始化动作状态机并请求站立姿态（上电默认站立） */
	Watchdog_Init();        /* 读取复位原因、配置并启动独立看门狗 IWDG */
	Face_Config();          /* 刷新初始表情与电量显示到 OLED */

	while (1)
	{
		const PetStatus *pet = PetState_Get();          /* 获取当前宠物状态指针（睡眠标志决定任务周期） */

		BlueTooth_Task();                               /* 持续执行：解析两个串口环形缓冲区中的命令 */
		if (Scheduler_Elapsed(&task20, 20)) PetAction_Task20ms();   /* 每20ms：舵机插值一步 + 动作关键帧推进 */
		if (Scheduler_Elapsed(&task100, 100))
		{
			PetState_Task100ms();                       /* 每100ms：尾巴摇动角度联动 */
			LED_Breathing();                            /* 每100ms：LED 常亮/呼吸/关闭 */
		}
		if (Scheduler_Elapsed(&task200, pet->sleeping ? 1000 : 200)) Face_Config();  /* 每200ms(睡眠时1000ms)：按需刷新 OLED */
		if (Scheduler_Elapsed(&task500, 500)) GetCur_Power();    /* 每500ms：采样电量并做两级平均滤波 */
		if (Scheduler_Elapsed(&task1000, pet->sleeping ? 5000 : 1000))
		{
			PetState_Task1s();                          /* 每1s(睡眠时5s)：心情/精力/睡眠/健康状态机 */
			BlueTooth_SendStatus();                     /* 每1s(睡眠时5s)：向蓝牙发送14字节状态遥测帧 */
		}

		Watchdog_Feed();                                /* 喂狗：防止程序卡死导致 IWDG 复位 */
		__WFI();                                        /* 空闲等待中断（SysTick/USART），降低功耗 */
	}
}
