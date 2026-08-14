/**
  ******************************************************************************
  * @file    Scheduler.c
  * @brief   裸机周期调度器：基于 SysTick 建立 1ms 时基，主循环据此分频执行任务
  *
  * 使用方式：
  *   1. main 启动时调用 Scheduler_Init()，SysTick 配置为 1ms 中断一次；
  *   2. 任意任务（含中断）通过 Scheduler_Millis() 获取上电以来的毫秒数；
  *   3. 主循环用 Scheduler_Elapsed(&last, period) 判断某个周期任务是否到点，
  *      到点则执行并更新 last 记录。所有任务共享这一个时基，互不阻塞。
  *
  * @note  无 RTOS，不涉及任务切换；SysTick 中断只做毫秒计数，开销极小
  ******************************************************************************
  */
#include "stm32f10x.h"
#include "Scheduler.h"

static volatile uint32_t system_ms;	/* 全局毫秒计数器：由 SysTick 中断累加，volatile 防止编译器优化 */

/**
  * @brief  初始化调度器：配置 SysTick 每 1ms 产生一次中断
  * @note   SystemCoreClock 为 72MHz，SysTick_Config(72MHz/1000) 即 1ms 中断
  */
void Scheduler_Init(void)
{
	SysTick_Config(SystemCoreClock / 1000);
}

/**
  * @brief  SysTick 中断服务函数（1ms 一次），只做一件事：毫秒计数 +1
  * @note   中断里尽量少做事，避免影响主循环的实时性
  */
void SysTick_Handler(void)
{
	system_ms++;
}

/**
  * @brief  获取系统上电以来的毫秒数（32 位溢出约 49.7 天后自动回绕，无需处理）
  * @retval 当前毫秒时间戳
  */
uint32_t Scheduler_Millis(void)
{
	return system_ms;
}

/**
  * @brief  周期判断核心函数：距上次执行是否已满 period_ms
  * @param  last      上次执行时刻的指针（由调用方保存，函数内部会更新）
  * @param  period_ms 任务周期（毫秒）
  * @retval 1 = 周期已到，本次应执行任务；0 = 未到，跳过
  * @note   使用无符号减法 (now - *last)，即使计数器回绕也能正确判断
  */
uint8_t Scheduler_Elapsed(uint32_t *last, uint32_t period_ms)
{
	uint32_t now = Scheduler_Millis();
	if ((uint32_t)(now - *last) < period_ms) return 0;
	*last = now;
	return 1;
}
