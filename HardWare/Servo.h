/**
  ******************************************************************************
  * @file    Servo.h
  * @brief   舵机驱动模块接口（见 Servo.c 详细说明）
  ******************************************************************************
  */
#ifndef __SERVO_H
#define __SERVO_H

#include "stm32f10x.h"

#define SERVO_COUNT 5   /* 舵机总数：4 腿 + 1 尾巴 */

void Servo_Init(void);                                                     /* 初始化 PWM 并置位到初始角度 */
void Servo_SetTargets(const uint8_t angles[SERVO_COUNT], uint16_t duration_ms); /* 设置 5 路目标角度与插值时长 */
void Servo_SetTarget(uint8_t index, uint8_t angle, uint16_t duration_ms);  /* 设置单路目标角度与插值时长 */
void Servo_Task20ms(void);                                                 /* 每 20ms 插值一步并输出 */
uint8_t Servo_GetCurrent(uint8_t index);                                   /* 获取某路当前实际角度 */
uint8_t Servo_GetTarget(uint8_t index);                                    /* 获取某路目标角度（遥测用） */
void Servo_SetCalibration(uint8_t index, uint8_t min_angle, uint8_t max_angle, int8_t trim); /* 设置限位与微调 */
void Servo_GetCalibration(uint8_t index, uint8_t *min_angle, uint8_t *max_angle, int8_t *trim); /* 读取校准参数 */
void Servo_Angle1(float angle);   /* 兼容旧版：左前腿直接定位 */
void Servo_Angle2(float angle);   /* 兼容旧版：右前腿直接定位 */
void Servo_Angle3(float angle);   /* 兼容旧版：左后腿直接定位 */
void Servo_Angle4(float angle);   /* 兼容旧版：右后腿直接定位 */
void WServo_Angle(float angle);   /* 兼容旧版：尾巴直接定位 */

#endif
