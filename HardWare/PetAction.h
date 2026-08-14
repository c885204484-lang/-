/**
  ******************************************************************************
  * @file    PetAction.h
  * @brief   非阻塞关键帧动作状态机接口（见 PetAction.c 详细说明）
  ******************************************************************************
  */
#ifndef __PET_ACTION_H
#define __PET_ACTION_H

#include "stm32f10x.h"

void PetAction_Init(void);                              /* 初始化动作系统并请求站立姿态 */
void PetAction_Request(uint8_t action, uint8_t sustained);   /* 请求执行动作(可打断当前动作)，sustained=持续执行 */
void PetAction_Task20ms(void);                          /* 每 20ms 任务：舵机插值 + 关键帧推进 */
void PetAction_Stop(void);                              /* 紧急停止：回到站立姿态（协议 0x62） */
uint8_t PetAction_IsBusy(void);                         /* 查询是否有动作正在执行 */
void PetAction_Perform(void);                           /* 兼容旧版：检测 Action_Mode 变化并请求新动作 */

#endif
