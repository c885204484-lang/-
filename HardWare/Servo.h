#ifndef __SERVO_H
#define __SERVO_H

#include "stm32f10x.h"

#define SERVO_COUNT 5

void Servo_Init(void);
void Servo_SetTargets(const uint8_t angles[SERVO_COUNT], uint16_t duration_ms);
void Servo_SetTarget(uint8_t index, uint8_t angle, uint16_t duration_ms);
void Servo_Task20ms(void);
uint8_t Servo_GetCurrent(uint8_t index);
uint8_t Servo_GetTarget(uint8_t index);
void Servo_SetCalibration(uint8_t index, uint8_t min_angle, uint8_t max_angle, int8_t trim);
void Servo_GetCalibration(uint8_t index, uint8_t *min_angle, uint8_t *max_angle, int8_t *trim);
void Servo_Angle1(float angle);
void Servo_Angle2(float angle);
void Servo_Angle3(float angle);
void Servo_Angle4(float angle);
void WServo_Angle(float angle);

#endif
