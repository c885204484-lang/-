/**
  ******************************************************************************
  * @file    PowerDetection.c
  * @brief   电量计算与滤波：把 ADC 原始值换算为百分比，并做两级平均
  *
  * 【换算公式】
  *   raw = (3.3V * 4倍分压 * ADC / 4095) * 100 - 300
  *   - 3.3V * 4：ADC 参考电压 3.3V，分压电路放大 4 倍
  *   - 结果表示"相对 3.0V 的百分之一伏增量"，如 15 ≈ 3.15V
  *   - raw >= 110 判定为"充电中"，<=105 取消充电标志（滞回防抖）
  *   - percent = raw * 100 / 120，映射到 0~100%
  *
  * 【两级滤波】
  *   第一级：AD.c 的 32 点 DMA 缓冲平均（硬件级）
  *   第二级：本文件 GetCur_Power() 对最近 8 次结果再做滑动平均，并取整到 20 的倍数
  ******************************************************************************
  */
#include "stm32f10x.h"
#include "AD.h"
#include "Variable.h"

/**
  * @brief  电量检测初始化（实际就是 ADC+DMA 初始化）
  */
void GetBattery_Init(void)
{
	AD_Init();
}

/**
  * @brief  单次电量换算：读取 ADC 平均原始值，换算成电量百分比
  * @retval 0~100 的电量百分比（同时写入全局 Battery_Value / Battery_Charging）
  */
uint16_t GetBattery(void)
{
	float raw = (3.3f * 4.0f * AD_GetAverage() / 4095.0f) * 100.0f - 300.0f;
	uint16_t percent;
	if (raw < 0) raw = 0;       /* 下限保护 */
	if (raw > 120) raw = 120;   /* 上限保护 */
	/* 充电判定：110~105 之间保持原状态（滞回，防止临界抖动） */
	if (raw >= 110) Battery_Charging = 1;
	else if (raw <= 105) Battery_Charging = 0;
	percent = (uint16_t)(raw * 100.0f / 120.0f + 0.5f);   /* 映射到百分比并四舍五入 */
	if (percent > 100) percent = 100;
	Battery_Value = percent;    /* 更新全局电量值 */
	return percent;
}

/**
  * @brief  电量周期任务（主循环每 500ms 调用）：
  *         对最近 8 次 GetBattery() 结果做滑动平均，并取整到 20 的倍数
  * @note   最终结果写入全局 CurBattery，供状态机/OLED/遥测使用
  */
void GetCur_Power(void)
{
	static uint16_t history[8];   /* 最近 8 次电量结果环形缓冲 */
	static uint8_t index;         /* 环形缓冲写指针 */
	static uint8_t count;         /* 已积累的有效样本数（前 8 次不满时取平均个数） */
	uint8_t i;
	uint32_t sum = 0;
	history[index++] = GetBattery();   /* 采样一次并写入环形缓冲 */
	if (index >= 8) index = 0;
	if (count < 8) count++;
	for (i = 0; i < count; i++) sum += history[i];        /* 滑动平均 */
	CurBattery = (uint16_t)(sum / count);
	CurBattery = (uint16_t)(((CurBattery + 10) / 20) * 20);   /* 四舍五入取整到 20 的倍数（20/40/60/80/100） */
	if (CurBattery > 100) CurBattery = 100;               /* 上限保护 */
}
