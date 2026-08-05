#include "stm32f10x.h"
#include "Scheduler.h"

static volatile uint32_t system_ms;// ① 全局毫秒计数器

void Scheduler_Init(void)
{
	SysTick_Config(SystemCoreClock / 1000);
}

void SysTick_Handler(void)// ② 1ms 中断只做一件事
{
	system_ms++;
}

uint32_t Scheduler_Millis(void)
{
	return system_ms;
}

uint8_t Scheduler_Elapsed(uint32_t *last, uint32_t period_ms)// ③ 核心函数
{
	uint32_t now = Scheduler_Millis();
	if ((uint32_t)(now - *last) < period_ms) return 0;
	*last = now;
	return 1;
}
