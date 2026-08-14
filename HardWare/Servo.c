/**
  ******************************************************************************
  * @file    Servo.c
  * @brief   舵机驱动：目标角度平滑插值、软件限位、零点微调、反向映射
  *
  * 【角度 -> PWM 占空比换算】
  *   PWM 周期 20ms(50Hz)，CCR 范围 500~2500 对应 0.5ms~2.5ms 脉宽，
  *   即 0°~180°：CCR = Angle / 180 * 2000 + 500
  *
  * 【5 路舵机定义】
  *   0: 左前腿(PA0/TIM2_CH1)   1: 右前腿(PA1/TIM2_CH2, 软件反向映射)
  *   2: 左后腿(PA2/TIM2_CH3)   3: 右后腿(PA3/TIM2_CH4, 软件反向映射)
  *   4: 尾巴(PA6/TIM3_CH1)
  *
  * 【平滑插值机制】
  *   Servo_SetTarget(s) 只更新目标角度和每 20ms 的步长 step_angle；
  *   真正的角度推进在 Servo_Task20ms() 中完成（由 PetAction_Task20ms 周期调用），
  *   因此舵机动作是非阻塞的，执行长动作期间仍能接收命令、刷新显示。
  *
  * 【校准】
  *   每路可单独设置最小/最大角度(默认10°~170°)与零点微调(±30°)，
  *   校准先只改内存，发送 0x61 命令后才写入 Flash（见 ConfigStore.c）
  ******************************************************************************
  */
#include "stm32f10x.h"
#include "PWM.h"
#include "Servo.h"

static float current_angle[SERVO_COUNT] = {90, 90, 90, 90, 90};  /* 当前实际角度（每20ms逼近目标） */
static float target_angle[SERVO_COUNT] = {90, 90, 90, 90, 90};   /* 目标角度 */
static float step_angle[SERVO_COUNT];                            /* 每20ms的插值步长（目标-当前)/帧数 */
static uint8_t min_angle[SERVO_COUNT] = {10, 10, 10, 10, 10};    /* 软件最小限位 */
static uint8_t max_angle[SERVO_COUNT] = {170, 170, 170, 170, 170}; /* 软件最大限位 */
static int8_t angle_trim[SERVO_COUNT];                           /* 零点微调(-30~+30) */

/**
  * @brief  角度限幅到 0~180 度
  */
static float Servo_Clamp(float angle)
{
	if (angle < 0) return 0;
	if (angle > 180) return 180;
	return angle;
}

/**
  * @brief  将某路舵机的角度换算为 PWM 比较值并写入对应 CCR 寄存器
  * @param  index 舵机编号 0~4
  * @param  angle 目标角度（含微调与限位处理）
  * @note   舵机1/3(右腿)机械方向相反，用 (180 - angle) 反向映射
  */
static void Servo_Write(uint8_t index, float angle)
{
	angle += angle_trim[index];                                  /* 叠加零点微调 */
	if (angle < min_angle[index]) angle = min_angle[index];      /* 下限位 */
	if (angle > max_angle[index]) angle = max_angle[index];      /* 上限位 */
	switch (index)
	{
		case 0: PWM_SetCompare1((uint16_t)(angle / 180.0f * 2000.0f + 500.0f)); break;         /* 左前腿：正向 */
		case 1: PWM_SetCompare2((uint16_t)((180.0f - angle) / 180.0f * 2000.0f + 500.0f)); break; /* 右前腿：反向 */
		case 2: PWM_SetCompare3((uint16_t)(angle / 180.0f * 2000.0f + 500.0f)); break;         /* 左后腿：正向 */
		case 3: PWM_SetCompare4((uint16_t)((180.0f - angle) / 180.0f * 2000.0f + 500.0f)); break; /* 右后腿：反向 */
		case 4: PWM_WSetCompare((uint16_t)(angle / 180.0f * 2000.0f + 500.0f)); break;         /* 尾巴：正向 */
	}
}

/**
  * @brief  舵机初始化：初始化 PWM 硬件，并把 5 路舵机写到初始角度(90°)
  */
void Servo_Init(void)
{
	uint8_t i;
	PWM_Init();
	for (i = 0; i < SERVO_COUNT; i++) Servo_Write(i, current_angle[i]);
}

/**
  * @brief  一次性设置 5 路舵机的目标角度
  * @param  angles      5 路目标角度数组
  * @param  duration_ms 从当前角度走到目标角度预计耗时（决定插值步长）
  * @note   每 20ms 前进一帧，帧数 = duration_ms / 20
  */
void Servo_SetTargets(const uint8_t angles[SERVO_COUNT], uint16_t duration_ms)
{
	uint8_t i;
	uint16_t ticks = duration_ms / 20;
	if (ticks == 0) ticks = 1;                                    /* 防止除零，至少 1 帧 */
	for (i = 0; i < SERVO_COUNT; i++)
	{
		target_angle[i] = Servo_Clamp(angles[i]);
		step_angle[i] = (target_angle[i] - current_angle[i]) / ticks;
	}
}

/**
  * @brief  设置单路舵机的目标角度
  * @param  index       舵机编号 0~4
  * @param  angle       目标角度 0~180
  * @param  duration_ms 插值耗时（毫秒）
  */
void Servo_SetTarget(uint8_t index, uint8_t angle, uint16_t duration_ms)
{
	uint16_t ticks = duration_ms / 20;
	if (index >= SERVO_COUNT) return;
	if (ticks == 0) ticks = 1;
	target_angle[index] = Servo_Clamp(angle);
	step_angle[index] = (target_angle[index] - current_angle[index]) / ticks;
}

/**
  * @brief  舵机 20ms 周期任务：所有舵机沿步长向目标角度推进一格并输出
  * @note   由 PetAction_Task20ms() 调用，实现非阻塞平滑运动
  */
void Servo_Task20ms(void)
{
	uint8_t i;
	for (i = 0; i < SERVO_COUNT; i++)
	{
		float delta = target_angle[i] - current_angle[i];   /* 剩余距离 */
		/* 剩余距离小于一步时直接到位，避免来回震荡 */
		if ((step_angle[i] >= 0 && delta <= step_angle[i]) || (step_angle[i] < 0 && delta >= step_angle[i])) 

			current_angle[i] = target_angle[i];

		else 
			current_angle[i] += step_angle[i];              /* 否则前进一格 */
		Servo_Write(i, current_angle[i]);
	}
}

/**
  * @brief  获取某路舵机当前实际角度
  */
uint8_t Servo_GetCurrent(uint8_t index)
{
	if (index >= SERVO_COUNT) return 0;
	return (uint8_t)current_angle[index];
}

/**
  * @brief  获取某路舵机目标角度（遥测帧使用）
  */
uint8_t Servo_GetTarget(uint8_t index)
{
	if (index >= SERVO_COUNT) return 0;
	return (uint8_t)target_angle[index];
}

/**
  * @brief  设置某路舵机的校准参数（限位与零点微调）
  * @param  index    舵机编号 0~4
  * @param  minimum  最小角度
  * @param  maximum  最大角度（须大于 minimum，且 <=180）
  * @param  trim     零点微调 -30~+30
  * @note   仅修改内存，需发送 0x61 命令才写入 Flash
  */
void Servo_SetCalibration(uint8_t index, uint8_t minimum, uint8_t maximum, int8_t trim)
{
	if (index >= SERVO_COUNT || minimum >= maximum || maximum > 180 || trim < -30 || trim > 30) return;
	min_angle[index] = minimum;
	max_angle[index] = maximum;
	angle_trim[index] = trim;
}

/**
  * @brief  读取某路舵机的校准参数（保存配置时使用）
  */
void Servo_GetCalibration(uint8_t index, uint8_t *minimum, uint8_t *maximum, int8_t *trim)
{
	if (index >= SERVO_COUNT) return;
	*minimum = min_angle[index];
	*maximum = max_angle[index];
	*trim = angle_trim[index];
}

/* 以下为兼容旧版代码的直接定位函数：立即把某路舵机转到指定角度（无插值） */
void Servo_Angle1(float angle) { current_angle[0] = target_angle[0] = Servo_Clamp(angle); Servo_Write(0, angle); }   /* 左前腿直接定位 */
void Servo_Angle2(float angle) { current_angle[1] = target_angle[1] = Servo_Clamp(angle); Servo_Write(1, angle); }   /* 右前腿直接定位 */
void Servo_Angle3(float angle) { current_angle[2] = target_angle[2] = Servo_Clamp(angle); Servo_Write(2, angle); }   /* 左后腿直接定位 */
void Servo_Angle4(float angle) { current_angle[3] = target_angle[3] = Servo_Clamp(angle); Servo_Write(3, angle); }   /* 右后腿直接定位 */
void WServo_Angle(float angle) { current_angle[4] = target_angle[4] = Servo_Clamp(angle); Servo_Write(4, angle); }   /* 尾巴直接定位 */
