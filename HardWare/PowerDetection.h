/**
  ******************************************************************************
  * @file    PowerDetection.h
  * @brief   电量计算与滤波模块接口（见 PowerDetection.c 详细说明）
  ******************************************************************************
  */
#ifndef __POWER_DETECTION_H_
#define __POWER_DETECTION_H_

void GetBattery_Init(void);     /* 电量检测初始化（实际为 ADC+DMA 初始化） */
uint16_t GetBattery(void);      /* 单次换算：ADC 原始值 -> 电量百分比(0~100) */
void GetCur_Power(void);        /* 每 500ms 任务：8 次滑动平均滤波，更新 CurBattery */
	
#endif
