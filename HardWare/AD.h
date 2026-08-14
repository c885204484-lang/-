/**
  ******************************************************************************
  * @file    AD.h
  * @brief   ADC1+DMA 电量采样模块接口（见 AD.c 详细说明）
  ******************************************************************************
  */
#ifndef __AD_H
#define __AD_H

#include "stm32f10x.h"

void AD_Init(void);                  /* ADC1_CH4 + DMA1_Channel1 循环采样初始化（TIM3 TRGO 触发） */
uint16_t AD_GetAverage(void);        /* 读取 32 点 DMA 缓冲的平均原始值(0~4095) */
uint16_t Get_ADC(void);              /* 兼容旧版接口：等价于 AD_GetAverage() */

#endif
