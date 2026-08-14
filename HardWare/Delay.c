/**
  ******************************************************************************
  * @file    Delay.c
  * @brief   阻塞式延时工具（基于 SysTick 直接操作寄存器）
  *
  * @note    本项目动作主流程已改为非阻塞调度（见 Scheduler.c / PetAction.c），
  *          这里的延时仅保留用于初始化、单次校准等阻塞场景。
  *          阻塞延时期间会占用 CPU，请勿在长动作中使用。
  ******************************************************************************
  */
#include "stm32f10x.h"

/**
  * @brief  微秒级延时
  * @param  xus 延时时长，范围：0~233015
  * @retval 无
  * @note   HCLK = 72MHz，每微秒计数 72 次；阻塞等待计数到 0 后关闭定时器
  */
void Delay_us(uint32_t xus)
{
	SysTick->LOAD = 72 * xus;				//设置定时器重装值
	SysTick->VAL = 0x00;					//清空当前计数值
	SysTick->CTRL = 0x00000005;				//设置时钟源为HCLK，启动定时器
	while(!(SysTick->CTRL & 0x00010000));	//等待计数到0
	SysTick->CTRL = 0x00000004;				//关闭定时器
}

/**
  * @brief  毫秒级延时
  * @param  xms 延时时长，范围：0~4294967295
  * @retval 无
  */
void Delay_ms(uint32_t xms)
{
	while(xms--)
	{
		Delay_us(1000);
	}
}
 
/**
  * @brief  秒级延时
  * @param  xs 延时时长，范围：0~4294967295
  * @retval 无
  */
void Delay_s(uint32_t xs)
{
	while(xs--)
	{
		Delay_ms(1000);
	}
} 
