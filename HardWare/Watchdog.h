/**
  ******************************************************************************
  * @file    Watchdog.h
  * @brief   独立看门狗模块接口（见 Watchdog.c 详细说明）
  ******************************************************************************
  */
#ifndef __WATCHDOG_H
#define __WATCHDOG_H

#include "stm32f10x.h"

void Watchdog_Init(void);                 /* 记录复位原因并启动 IWDG（约 2 秒超时） */
void Watchdog_Feed(void);                 /* 喂狗：主循环每轮调用，防止复位 */
uint8_t Watchdog_WasReset(void);          /* 查询本次启动是否源于看门狗复位 */

#endif
