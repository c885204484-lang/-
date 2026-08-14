/**
  ******************************************************************************
  * @file    PetState.c
  * @brief   虚拟宠物状态机：心情(mood)/精力(energy)/活跃度(activity)/睡眠/健康联动
  *
  * 【状态取值范围】
  *   心情 mood      0~100：互动时提高，长时间无人互动时下降
  *   精力 energy    0~100：运动时消耗，静止时恢复
  *   活跃度 activity 10~90：控制尾巴摇动速度
  *   睡眠 sleeping  0/1  ：自动趴下并降低非关键任务频率（见 main.c）
  *   健康 health    正常/低电量/精力不足：限制高耗能动作
  *
  * 【联动规则】
  *   - 收到有效命令：唤醒、心情+5、按命令请求动作
  *   - 连续 SLEEP_TIMEOUT_MS(120s) 无互动：显示睡觉表情、趴下、开呼吸灯
  *   - 移动类动作：精力-1/s、活跃度+5/s；静止时：精力+1/s、活跃度回落
  *   - 低电量(CurBattery<=15)或低精力(energy<15)：拒绝跳跃/摇摆并转坐下
  *   - 状态页(0x60)显示 5 秒后自动退出
  ******************************************************************************
  */
#include "stm32f10x.h"
#include "PetState.h"
#include "PetAction.h"
#include "Scheduler.h"
#include "Variable.h"
#include "Servo.h"
#include "ConfigStore.h"
#include "Led_Breathing.h"

#define SLEEP_TIMEOUT_MS 120000UL   /* 自动入睡等待时间：120 秒无互动 */
#define LOW_BATTERY_LEVEL 15        /* 低电量阈值（电量值<=15 视为低电量） */
#define LOW_ENERGY_LEVEL 15         /* 低精力阈值 */

static PetStatus status;            /* 宠物状态结构体（唯一实例） */
static uint8_t tail_angle = 90;     /* 尾巴当前角度（摇尾用） */
static int8_t tail_direction = 1;   /* 摇尾方向：1=向高角度摆，-1=向低角度摆 */

/**
  * @brief  判断动作是否为"移动类"（会消耗精力、提高活跃度）
  */
static uint8_t IsMotion(uint8_t action)
{
	return action >= 4 && action <= 15 && action != 9;
}

/**
  * @brief  宠物状态初始化：从 Flash 配置恢复心情/精力，其余取默认值
  */
void PetState_Init(void)
{
	const PetConfig *config = ConfigStore_Get();
	status.mood = config->mood;                 /* 从上次保存的配置恢复心情 */
	status.energy = config->energy;             /* 从上次保存的配置恢复精力 */
	status.activity = 50;                       /* 活跃度初始为中间值 */
	status.sleeping = 0;                        /* 上电默认清醒 */
	status.health = PET_HEALTH_OK;              /* 健康状态正常 */
	status.last_interaction_ms = Scheduler_Millis();  /* 记录最后互动时间（用于自动入睡） */
	status.uptime_seconds = 0;                  /* 运行时长计数 */
}

/**
  * @brief  处理一条有效命令：唤醒宠物、提升心情，并映射为动作请求
  * @param  command   命令字节（0x28~0x50，见 README 协议表）
  * @param  sustained 是否持续执行（蓝牙通道移动命令为 1）
  * @retval 1=命令接受；0=命令被拒绝（如低电量/低精力时跳跃）
  */
uint8_t PetState_Command(uint8_t command, uint8_t sustained)
{
	uint8_t action = 0xFF;
	status.last_interaction_ms = Scheduler_Millis();  /* 有互动：刷新最后互动时间 */
	status.sleeping = 0;                              /* 唤醒宠物 */
	Status_Display_Bit = 0;                           /* 退出状态显示页 */
	if (status.mood < 95) status.mood += 5;           /* 互动提高心情（封顶95） */

	switch (command)
	{
		case 0x28: action = 2; Face_Mode = 5; break;              /* 站立：普通眼睛 */
		case 0x29: action = 0; Face_Mode = 0; status.sleeping = 1; break;  /* 放松趴下：睡觉表情（手动休息） */
		case 0x30: action = 1; Face_Mode = 1; break;              /* 坐下：瞪眼 */
		case 0x31: action = 2; Face_Mode = 5; break;              /* 站立：普通眼睛 */
		case 0x32: action = 3; Face_Mode = 1; break;              /* 趴下：瞪眼 */
		case 0x33: action = 4; Face_Mode = 2; break;              /* 前进：快乐 */
		case 0x34: action = 5; Face_Mode = 2; break;              /* 后退：快乐 */
		case 0x35: action = 6; Face_Mode = 2; break;              /* 左转：快乐 */
		case 0x36: action = 7; Face_Mode = 2; break;              /* 右转：快乐 */
		case 0x37: action = 8; Face_Mode = 4; break;              /* 全身摇摆：非常快乐 */
		case 0x38:   /* 调整移动速度：200->180->...->100->200 循环 */
			if (SpeedDelay > 100) SpeedDelay -= 20; else SpeedDelay = 200;
			return 1;
		case 0x39:   /* 调整摇摆延时：递减，到最小值恢复 9ms */
			if (SwingDelay > 3) SwingDelay--; else SwingDelay = 9;
			return 1;
		case 0x40:   /* 开关自动摇尾 */
			WeiBa_Bit ^= 1;
			if (!WeiBa_Bit) { tail_angle = 90; tail_direction = 1; Servo_SetTarget(4, 90, 150); }  /* 关闭时尾巴回中 */
			return 1;
		case 0x41: action = 10; Face_Mode = 2; break;             /* 向前跳：快乐 */
		case 0x42: action = 11; Face_Mode = 2; break;             /* 向后跳：快乐 */
		case 0x43: action = 13; Face_Mode = 6; break;             /* 打招呼：Hello 表情 */
		case 0x44: AllLed = 1; LED_Breathing(); return 1;         /* LED 全开 */
		case 0x45: AllLed = 0; LED_Breathing(); return 1;         /* LED 关闭 */
		case 0x46: BreatheLed = 1; PanDuan = 1; HuXi = 0; Wait = 0; LED_Breathing(); return 1;  /* 开启呼吸灯并复位呼吸相位 */
		case 0x47: BreatheLed = 0; LED_Breathing(); return 1;     /* 关闭呼吸灯 */
		case 0x48: action = 14; Face_Mode = 6; break;             /* 伸懒腰：Hello 表情 */
		case 0x49: action = 15; Face_Mode = 6; break;             /* 后腿拉伸：Hello 表情 */
		case 0x50: Battery_Bit ^= 1; return 1;                    /* 切换电量显示开关 */
		default: return 0;                                        /* 未知命令 */
	}

	/* 健康限制：低电量或低精力时拒绝高耗能动作（跳跃/摇摆），改为坐下并瞪眼 */
	if ((status.health == PET_HEALTH_LOW_BATTERY || status.energy < LOW_ENERGY_LEVEL) &&
		(action == 10 || action == 11 || action == 8))
	{
		Face_Mode = 1;
		PetAction_Request(1, 0);
		return 0;
	}
	/* 正常执行：蓝牙通道的移动动作(4~7)支持持续执行 */
	PetAction_Request(action, (uint8_t)(sustained && action >= 4 && action <= 7));
	return 1;
}

/**
  * @brief  状态 1 秒任务（清醒时每 1s，睡眠时每 5s 调用）：
  *         健康判断、精力/活跃度演化、自动入睡、心情缓慢下降
  */
void PetState_Task1s(void)
{
	uint32_t idle_ms;
	status.uptime_seconds++;                  /* 运行秒数 +1 */

	/* 状态显示页(0x60 查询)超时 5 秒后自动退出，回到站立/待机表情 */
	if (Status_Display_Seconds > 0)
	{
		Status_Display_Seconds--;
		if (Status_Display_Seconds == 0)
		{
			Status_Display_Bit = 0;
			Face_Mode = 5; /*状态页超时后回到站立/待机表情*/
		}
	}
	idle_ms = Scheduler_Millis() - status.last_interaction_ms;   /* 距上次互动的毫秒数 */

	/* ---- 健康状态判定 ---- */
	if (CurBattery > 0 && CurBattery <= LOW_BATTERY_LEVEL)
	{
		/* 低电量：关灯、瞪眼表情，若在动则强制趴下进入保护姿态 */
		status.health = PET_HEALTH_LOW_BATTERY;
		AllLed = 0;
		Face_Mode = 1;
		if (PetAction_IsBusy()) PetAction_Request(0, 0);
	}
	else if (status.energy < LOW_ENERGY_LEVEL) status.health = PET_HEALTH_EXHAUSTED;  /* 精力不足 */
	else status.health = PET_HEALTH_OK;                                                /* 正常 */

	/* ---- 精力/活跃度演化 ---- */
	if (IsMotion((uint8_t)Action_Mode) && PetAction_IsBusy())
	{
		/* 运动中：消耗精力，提高活跃度 */
		if (status.energy > 0) status.energy--;
		if (status.activity <= 95) status.activity += 5;
		else status.activity = 100;
	}
	else
	{
		/* 静止中：恢复精力，活跃度缓慢回落（睡眠时回落到更低水平） */
		uint8_t minimum_activity = status.sleeping ? 10 : 35;
		if (status.energy < 100) status.energy++;
		if (status.activity > minimum_activity)
		{
			if (status.activity >= minimum_activity + 2) status.activity -= 2;
			else status.activity = minimum_activity;
		}
	}

	/* ---- 自动入睡：超过 SLEEP_TIMEOUT_MS 无互动 ---- */
	if (idle_ms >= SLEEP_TIMEOUT_MS && !status.sleeping)
	{
		status.sleeping = 1;
		Face_Mode = 0;          /* 睡觉表情 */
		BreatheLed = 1;         /* 睡觉时开呼吸灯 */
		PetAction_Request(0, 0);/* 趴下 */
	}
	/* ---- 心情缓慢下降：无人互动超过 30 秒，心情每 1s -1 ---- */
	if (idle_ms > 30000 && status.mood > 10) status.mood--;
}

/**
  * @brief  获取宠物状态结构体指针（只读访问）
  */
const PetStatus *PetState_Get(void)
{
	return &status;
}

/**
  * @brief  状态 100ms 任务：自动摇尾巴
  * @note   开启条件：WeiBa_Bit=1（0x40 开启）且未睡觉且电量正常；
  *         活跃度>=70 时大步幅(20°)，否则小步幅(10°)；
  *         尾巴在 35°~145° 之间往返摆动
  */
void PetState_Task100ms(void)
{
	uint8_t step;
	if (!WeiBa_Bit || status.sleeping || status.health == PET_HEALTH_LOW_BATTERY) return;
	step = status.activity >= 70 ? 20 : 10;
	if (tail_direction > 0)
	{
		if (tail_angle + step >= 145) { tail_angle = 145; tail_direction = -1; }  /* 到上限反向 */
		else tail_angle += step;
	}
	else
	{
		if (tail_angle <= 35 + step) { tail_angle = 35; tail_direction = 1; }     /* 到下限反向 */
		else tail_angle -= step;
	}
	Servo_SetTarget(4, tail_angle, 100);   /* 100ms 内到达，平滑摆动 */
}
