/**
  ******************************************************************************
  * @file    Scheduler.h
  * @brief   裸机周期调度器接口（见 Scheduler.c 详细说明）
  ******************************************************************************
  */
#ifndef __SCHEDULER_H
#define __SCHEDULER_H

#include "stm32f10x.h"

void Scheduler_Init(void);                              /* 启动 SysTick 1ms 时基 */
uint32_t Scheduler_Millis(void);                        /* 获取上电以来的毫秒数 */
uint8_t Scheduler_Elapsed(uint32_t *last, uint32_t period_ms);   /* 周期判断：距上次是否满 period_ms */

#endif
