#include "stm32f10x.h"
#include "AD.h"
#include "Variable.h"

void GetBattery_Init(void)
{
	AD_Init();
}

uint16_t GetBattery(void)
{
	float value = (3.3f * 4.0f * AD_GetAverage() / 4095.0f) * 100.0f - 300.0f;
	if (value < 0) value = 0;
	if (value > 999) value = 999;
	Battery_Value = value;
	return (uint16_t)value;
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
}
