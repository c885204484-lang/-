#include "stm32f10x.h"
#include "Watchdog.h"

static uint8_t watchdog_reset;

void Watchdog_Init(void)
{
	watchdog_reset = RCC_GetFlagStatus(RCC_FLAG_IWDGRST) == SET;
	RCC_ClearFlag();
	IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);
	IWDG_SetPrescaler(IWDG_Prescaler_64);
	IWDG_SetReload(1250);
	IWDG_ReloadCounter();
	IWDG_Enable();
}

void Watchdog_Feed(void)
{
	IWDG_ReloadCounter();
}

uint8_t Watchdog_WasReset(void)
{
	return watchdog_reset;
}
