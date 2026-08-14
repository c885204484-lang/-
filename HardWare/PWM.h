/**
  ******************************************************************************
  * @file    PWM.h
  * @brief   PWM 模块接口（见 PWM.c 详细说明）
  * @note    TIM2_CH1~4：4 路腿部舵机；TIM3_CH1：尾巴；TIM3_CH3/4：LED1/LED2
  ******************************************************************************
  */
#ifndef __PWM_H
#define __PWM_H

#include "stm32f10x.h"

void PWM_Init(void);                     /* TIM2/TIM3 PWM 初始化（50Hz，周期20ms） */
void PWM_SetCompare1(uint16_t Compare);  /* 舵机1 左前腿(PA0/TIM2_CH1) 占空比 */
void PWM_SetCompare2(uint16_t Compare);  /* 舵机2 右前腿(PA1/TIM2_CH2) 占空比 */
void PWM_SetCompare3(uint16_t Compare);  /* 舵机3 左后腿(PA2/TIM2_CH3) 占空比 */
void PWM_SetCompare4(uint16_t Compare);  /* 舵机4 右后腿(PA3/TIM2_CH4) 占空比 */
void PWM_WSetCompare(uint16_t Compare);  /* 尾巴舵机(PA6/TIM3_CH1) 占空比 */
void PWM_LED1(uint16_t Compare);         /* LED1(PB0/TIM3_CH3) 亮度 */
void PWM_LED2(uint16_t Compare);         /* LED2(PB1/TIM3_CH4) 亮度 */

#endif
