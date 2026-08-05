#include "stm32f10x.h"
#include "PWM.h"
#include "Servo.h"

static float current_angle[SERVO_COUNT] = {90, 90, 90, 90, 90};
static float target_angle[SERVO_COUNT] = {90, 90, 90, 90, 90};
static float step_angle[SERVO_COUNT];
static uint8_t min_angle[SERVO_COUNT] = {10, 10, 10, 10, 10};
static uint8_t max_angle[SERVO_COUNT] = {170, 170, 170, 170, 170};
static int8_t angle_trim[SERVO_COUNT];

static float Servo_Clamp(float angle)
{
	if (angle < 0) return 0;
	if (angle > 180) return 180;
	return angle;
}

static void Servo_Write(uint8_t index, float angle)
{
	angle += angle_trim[index];
	if (angle < min_angle[index]) angle = min_angle[index];
	if (angle > max_angle[index]) angle = max_angle[index];
	switch (index)
	{
		case 0: PWM_SetCompare1((uint16_t)(angle / 180.0f * 2000.0f + 500.0f)); break;
		case 1: PWM_SetCompare2((uint16_t)((180.0f - angle) / 180.0f * 2000.0f + 500.0f)); break;
		case 2: PWM_SetCompare3((uint16_t)(angle / 180.0f * 2000.0f + 500.0f)); break;
		case 3: PWM_SetCompare4((uint16_t)((180.0f - angle) / 180.0f * 2000.0f + 500.0f)); break;
		case 4: PWM_WSetCompare((uint16_t)(angle / 180.0f * 2000.0f + 500.0f)); break;
	}
}

void Servo_Init(void)
{
	uint8_t i;
	PWM_Init();
	for (i = 0; i < SERVO_COUNT; i++) Servo_Write(i, current_angle[i]);
}

void Servo_SetTargets(const uint8_t angles[SERVO_COUNT], uint16_t duration_ms)
{
	uint8_t i;
	uint16_t ticks = duration_ms / 20;
	if (ticks == 0) ticks = 1;
	for (i = 0; i < SERVO_COUNT; i++)
	{
		target_angle[i] = Servo_Clamp(angles[i]);
		step_angle[i] = (target_angle[i] - current_angle[i]) / ticks;
	}
}

void Servo_SetTarget(uint8_t index, uint8_t angle, uint16_t duration_ms)
{
	uint16_t ticks = duration_ms / 20;
	if (index >= SERVO_COUNT) return;
	if (ticks == 0) ticks = 1;
	target_angle[index] = Servo_Clamp(angle);
	step_angle[index] = (target_angle[index] - current_angle[index]) / ticks;
}

void Servo_Task20ms(void)
{
	uint8_t i;
	for (i = 0; i < SERVO_COUNT; i++)
	{
		float delta = target_angle[i] - current_angle[i];
		if ((step_angle[i] >= 0 && delta <= step_angle[i]) || (step_angle[i] < 0 && delta >= step_angle[i])) 

			current_angle[i] = target_angle[i];

		else 
			current_angle[i] += step_angle[i];
		Servo_Write(i, current_angle[i]);
	}
}

uint8_t Servo_GetCurrent(uint8_t index)
{
	if (index >= SERVO_COUNT) return 0;
	return (uint8_t)current_angle[index];
}

uint8_t Servo_GetTarget(uint8_t index)
{
	if (index >= SERVO_COUNT) return 0;
	return (uint8_t)target_angle[index];
}

void Servo_SetCalibration(uint8_t index, uint8_t minimum, uint8_t maximum, int8_t trim)
{
	if (index >= SERVO_COUNT || minimum >= maximum || maximum > 180 || trim < -30 || trim > 30) return;
	min_angle[index] = minimum;
	max_angle[index] = maximum;
	angle_trim[index] = trim;
}

void Servo_GetCalibration(uint8_t index, uint8_t *minimum, uint8_t *maximum, int8_t *trim)
{
	if (index >= SERVO_COUNT) return;
	*minimum = min_angle[index];
	*maximum = max_angle[index];
	*trim = angle_trim[index];
}

void Servo_Angle1(float angle) { current_angle[0] = target_angle[0] = Servo_Clamp(angle); Servo_Write(0, angle); }
void Servo_Angle2(float angle) { current_angle[1] = target_angle[1] = Servo_Clamp(angle); Servo_Write(1, angle); }
void Servo_Angle3(float angle) { current_angle[2] = target_angle[2] = Servo_Clamp(angle); Servo_Write(2, angle); }
void Servo_Angle4(float angle) { current_angle[3] = target_angle[3] = Servo_Clamp(angle); Servo_Write(3, angle); }
void WServo_Angle(float angle) { current_angle[4] = target_angle[4] = Servo_Clamp(angle); Servo_Write(4, angle); }
