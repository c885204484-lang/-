/**
  ******************************************************************************
  * @file    Watchdog.c
  * @brief   独立看门狗(IWDG)：程序卡死时自动复位 MCU，并记录复位原因上报
  *
  * 【工作原理】
  *   IWDG 由 LSI(约40kHz) 驱动，不依赖主时钟：
  *   分频 64 -> 计数频率约 625Hz；重装值 1250 -> 超时约 2 秒
  *   主循环每轮调用 Watchdog_Feed() 重装计数器；
  *   若主循环卡死超过约 2 秒未喂狗，IWDG 强制复位系统。
  *
  * 【复位原因】上电时读取 RCC_FLAG_IWDGRST：
  *   1 = 本次启动源于看门狗复位（说明之前程序卡死过），
  *   该标志随状态遥测帧(0x70)上报给上位机。
  ******************************************************************************
  */
#include "stm32f10x.h"
#include "Watchdog.h"

static uint8_t watchdog_reset;   /* 本次启动是否源于 IWDG 复位（0/1） */

/**
  * @brief  看门狗初始化：记录复位原因并启动 IWDG
  * @note   DBGMCU 暂停看门狗：调试器连接/烧录时停止计数，
  *         否则在调试中断点期间 IWDG 超时会导致 Flash Download Failed
  */
void Watchdog_Init(void)
{
	/* 读取并清除复位标志：IWDGRST 置位说明上次是被看门狗复位的 */
	watchdog_reset = RCC_GetFlagStatus(RCC_FLAG_IWDGRST) == SET;
	RCC_ClearFlag();

	/* 调试时暂停看门狗，否则烧录时 IWDG 超时会导致 Flash Download Failed */
	DBGMCU_Config(DBGMCU_IWDG_STOP, ENABLE);

	/* 配置 IWDG：分频64，重装值1250（约2秒超时） */
	IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);   /* 使能对 IWDG 寄存器的写访问（只能写一次） */
	IWDG_SetPrescaler(IWDG_Prescaler_64);
	IWDG_SetReload(1250);
	IWDG_ReloadCounter();                           /* 先重装一次计数器 */
	IWDG_Enable();                                  /* 启动看门狗 */
}

/**
  * @brief  喂狗：重装 IWDG 计数器（主循环每轮调用一次）
  * @note   必须在超时时间(约2s)内至少调用一次，否则系统复位
  */
void Watchdog_Feed(void)
{
	IWDG_ReloadCounter();
}

/**
  * @brief  查询本次启动是否由看门狗复位引起
  * @retval 1=是（程序曾卡死）；0=否（正常上电或其他复位）
  */
uint8_t Watchdog_WasReset(void)
{
	return watchdog_reset;
}
