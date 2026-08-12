#include "stm32f10x.h"
#include "AD.h"
#include "Variable.h"

void GetBattery_Init(void)
{
	AD_Init();
}

uint16_t GetBattery(void)
{
	float raw = (3.3f * 4.0f * AD_GetAverage() / 4095.0f) * 100.0f - 300.0f;
	uint16_t percent;
	if (raw < 0) raw = 0;
	if (raw > 120) raw = 120;
	if (raw >= 110) Battery_Charging = 1;
	else if (raw <= 105) Battery_Charging = 0;
	percent = (uint16_t)(raw * 100.0f / 120.0f + 0.5f);
	if (percent > 100) percent = 100;
	Battery_Value = percent;
	return percent;
}

void GetCur_Power(void)
{
	static uint16_t history[8];
	static uint8_t index;
	static uint8_t count;
	uint8_t i;
	uint32_t sum = 0;
	history[index++] = GetBattery();
	if (index >= 8) index = 0;
	if (count < 8) count++;
	for (i = 0; i < count; i++) sum += history[i];
	CurBattery = (uint16_t)(sum / count);
	CurBattery = (uint16_t)(((CurBattery + 10) / 20) * 20);
	if (CurBattery > 100) CurBattery = 100;
}
