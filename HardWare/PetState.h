/**
  ******************************************************************************
  * @file    PetState.h
  * @brief   虚拟宠物状态机接口（见 PetState.c 详细说明）
  ******************************************************************************
  */
#ifndef __PET_STATE_H
#define __PET_STATE_H

#include "stm32f10x.h"

/* 宠物健康状态枚举 */
typedef enum
{
	PET_HEALTH_OK = 0,           /* 正常 */
	PET_HEALTH_LOW_BATTERY = 1,  /* 低电量 */
	PET_HEALTH_EXHAUSTED = 2     /* 精力不足 */
} PetHealth;

/* 宠物状态结构体 */
typedef struct
{
	uint8_t mood;                 /* 心情 0~100 */
	uint8_t energy;               /* 精力 0~100 */
	uint8_t activity;             /* 活跃度 10~90（控制尾巴摆动速度） */
	uint8_t sleeping;             /* 睡眠标志 0/1 */
	PetHealth health;             /* 健康状态 */
	uint32_t last_interaction_ms; /* 最后互动时刻(ms)，用于自动入睡判断 */
	uint32_t uptime_seconds;      /* 上电运行秒数 */
} PetStatus;

void PetState_Init(void);                              /* 状态初始化（从 Flash 配置恢复心情/精力） */
void PetState_Task1s(void);                            /* 每 1s 任务：健康/精力/睡眠/心情演化 */
void PetState_Task100ms(void);                         /* 每 100ms 任务：自动摇尾巴 */
uint8_t PetState_Command(uint8_t command, uint8_t sustained);   /* 处理一条有效命令，返回 1=接受 0=拒绝 */
const PetStatus *PetState_Get(void);                   /* 获取当前状态结构体只读指针 */

#endif
