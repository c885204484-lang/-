/**
  ******************************************************************************
  * @file    ConfigStore.h
  * @brief   Flash 配置存储模块接口（见 ConfigStore.c 详细说明）
  ******************************************************************************
  */
#ifndef __CONFIG_STORE_H
#define __CONFIG_STORE_H

#include "stm32f10x.h"

/* 业务配置结构体：保存到 Flash 的最后一个 1KB 页面(0x0800FC00) */
typedef struct
{
	uint16_t speed_delay;        /* 移动速度(ms/帧)，范围 80~400 */
	uint16_t swing_delay;        /* 摇摆延时，范围 2~20 */
	uint8_t led_enabled;         /* LED 开关(0/1) */
	uint8_t breathe_enabled;     /* 呼吸灯开关(0/1) */
	uint8_t battery_visible;     /* OLED 电量显示开关(0/1) */
	uint8_t mood;                /* 宠物心情 0~100 */
	uint8_t energy;              /* 宠物精力 0~100 */
	uint8_t servo_min[5];        /* 5 路舵机最小角度 */
	uint8_t servo_max[5];        /* 5 路舵机最大角度 */
	int8_t servo_trim[5];        /* 5 路舵机零点微调(-30~+30) */
	uint8_t reserved[3];         /* 保留字节（对齐/扩展用） */
} PetConfig;

void ConfigStore_Load(void);                 /* 上电加载配置（校验失败则用默认值），须在 Servo_Init 前调用 */
uint8_t ConfigStore_Save(void);              /* 保存当前参数到 Flash（协议 0x61），返回 1=成功 */
const PetConfig *ConfigStore_Get(void);      /* 获取当前生效配置的只读指针 */

#endif
