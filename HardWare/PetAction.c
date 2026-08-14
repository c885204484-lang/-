/**
  ******************************************************************************
  * @file    PetAction.c
  * @brief   非阻塞关键帧动作状态机：用若干关键帧描述一个动作，每 20ms 推进一帧
  *
  * 【关键帧结构】
  *   ActionFrame { uint8_t angle[5]; uint16_t duration_ms; }
  *   - angle[5]      ：5 路舵机(4腿+尾巴)在该帧的目标角度
  *   - duration_ms   ：该帧持续时长（移动类动作改用 SpeedDelay，见 FrameDuration）
  *
  * 【工作机制】
  *   1. PetAction_Request(action, sustained) 收到命令后立即替换当前动作，
  *      因此前进/摇摆/招手等过程中可以随时被新命令打断；
  *   2. PetAction_Task20ms() 每 20ms 由主循环调用：
  *      - 先调 Servo_Task20ms() 让 5 路舵机向当前帧目标平滑插值一步；
  *      - 当前帧播放满 duration_ms 后切换到下一帧；
  *      - 播完最后一帧：持续命令或还有重复次数则循环，否则回到站立姿态。
  *
  * 【动作编号映射】（Action_Mode，与旧版协议兼容）
  *   0 睡觉姿态  1 坐下  2 站立  3 趴下
  *   4 前进      5 后退  6 左转  7 右转
  *   8 全身摇摆  9 (保留) 10 向前跳 11 向后跳
  *   12 站立(同2) 13 打招呼 14 伸懒腰 15 后腿拉伸
  ******************************************************************************
  */
#include "stm32f10x.h"
#include "PetAction.h"
#include "Servo.h"
#include "Variable.h"

/* 关键帧数据结构：一帧 = 5 路舵机目标角度 + 帧持续时间 */
typedef struct
{
	uint8_t angle[SERVO_COUNT];
	uint16_t duration_ms;
} ActionFrame;

/*--------------------- 动作关键帧表（静态只读，节省 RAM） ---------------------*/
static const ActionFrame pose_sleep[] = {{{20,20,160,160,90}, 500}};        /* 动作0：睡觉（四腿蜷缩） */
static const ActionFrame pose_sit[] = {{{90,90,20,20,90}, 400}};            /* 动作1：坐下（前腿伸直） */
static const ActionFrame pose_stand[] = {{{90,90,90,90,90}, 400}};          /* 动作2：站立（默认姿态） */
static const ActionFrame pose_down[] = {{{20,20,20,20,90}, 400}};           /* 动作3：趴下（四腿前伸） */
/* 动作4：前进 —— 8 帧构成一个完整步态周期 */
static const ActionFrame move_forward[] = {
	{{90,45,45,90,90},200}, {{135,45,45,135,90},200}, {{135,90,90,135,90},200}, {{90,90,90,90,90},200},
	{{45,90,90,45,90},200}, {{45,135,135,45,90},200}, {{90,135,135,90,90},200}, {{90,90,90,90,90},200}
};
/* 动作5：后退 —— 步态与前进相反 */
static const ActionFrame move_back[] = {
	{{90,135,135,90,90},200}, {{45,135,135,45,90},200}, {{45,90,90,45,90},200}, {{90,90,90,90,90},200},
	{{135,90,90,135,90},200}, {{135,45,45,135,90},200}, {{90,45,45,90,90},200}, {{90,90,90,90,90},200}
};
/* 动作6：左转 */
static const ActionFrame turn_left[] = {{{90,45,135,90,90},200},{{45,45,135,135,90},200},{{45,90,90,135,90},200},{{90,90,90,90,90},200}};
/* 动作7：右转 */
static const ActionFrame turn_right[] = {{{45,90,90,135,90},200},{{45,45,135,135,90},200},{{90,45,135,90,90},200},{{90,90,90,90,90},200}};
/* 动作8：全身摇摆 */
static const ActionFrame swing[] = {{{30,30,30,30,50},500},{{150,150,150,150,130},900},{{30,30,30,30,50},900},{{90,90,90,90,90},500}};
/* 动作10：向前跳 */
static const ActionFrame jump_forward[] = {{{140,90,90,35,90},250},{{140,140,35,35,90},280},{{90,90,90,90,90},400}};
/* 动作11：向后跳 */
static const ActionFrame jump_back[] = {{{140,90,90,35,90},250},{{140,140,35,35,90},280},{{90,90,90,90,90},400}};
/* 动作13：打招呼（尾巴摆动） */
static const ActionFrame hello[] = {{{90,20,20,45,110},350},{{90,55,20,45,140},300},{{90,10,20,45,70},300},{{90,55,20,45,140},300},{{90,90,90,90,90},450}};
/* 动作14：伸懒腰 */
static const ActionFrame stretch[] = {{{10,10,90,90,90},700},{{90,90,170,170,90},700},{{90,90,90,90,90},500}};
/* 动作15：后腿拉伸 */
static const ActionFrame leg_stretch[] = {{{90,20,170,110,90},700},{{90,90,90,90,90},400},{{20,90,110,170,90},700},{{90,90,90,90,90},500}};

/*--------------------- 状态机内部变量 ---------------------*/
static const ActionFrame *frames;      /* 当前动作的关键帧表指针 */
static uint8_t frame_count;            /* 当前动作总帧数 */
static uint8_t frame_index;            /* 当前播放到第几帧 */
static uint16_t frame_elapsed;         /* 当前帧已播放的毫秒数 */
static uint8_t active_action;          /* 当前正在执行的动作编号，0xFF 表示空闲 */
static uint8_t repeat_left;            /* 剩余重复次数 */
static uint8_t sustained_action;       /* 是否持续执行（蓝牙通道的移动动作） */

/**
  * @brief  计算当前帧的持续时长
  * @retval 移动类动作(4~7：前进/后退/左转/右转)使用 SpeedDelay 可调速度，
  *         其余动作使用关键帧自带的 duration_ms
  */
static uint16_t FrameDuration(void)
{
	if (active_action >= 4 && active_action <= 7) 
		return SpeedDelay;
	return frames[frame_index].duration_ms;
}

/**
  * @brief  根据动作编号选择关键帧表，并初始化帧数/重复次数
  * @param  action 动作编号（见文件头注释）
  * @note   action == 9 为保留值，不在此处理
  */
static void SelectAction(uint8_t action)
{
	frames = pose_stand;   /* 默认先指向站立姿态 */
	frame_count = 1; 
	repeat_left = 1;
	switch (action)
	{
		case 0: frames = pose_sleep; frame_count = 1; break;                          /* 睡觉 */
		case 1: frames = pose_sit; frame_count = 1; break;                            /* 坐下 */
		case 2: frames = pose_stand; frame_count = 1; break;                          /* 站立 */
		case 3: frames = pose_down; frame_count = 1; break;                           /* 趴下 */
		case 4: frames = move_forward; frame_count = 8; repeat_left = Chongfunumber; break;   /* 前进 */
		case 5: frames = move_back; frame_count = 8; repeat_left = Chongfunumber; break;      /* 后退 */
		case 6: frames = turn_left; frame_count = 4; repeat_left = Chongfunumber * 2; break;  /* 左转 */
		case 7: frames = turn_right; frame_count = 4; repeat_left = Chongfunumber * 2; break; /* 右转 */
		case 8: frames = swing; frame_count = 4; repeat_left = SwingRepeatnumber; break;      /* 摇摆 */
		case 10: frames = jump_forward; frame_count = 3; break;                       /* 向前跳 */
		case 11: frames = jump_back; frame_count = 3; break;                          /* 向后跳 */
		case 12: frames = pose_stand; frame_count = 1; break;                         /* 站立（等价2） */
		case 13: frames = hello; frame_count = 5; repeat_left = HelloRepeatnumber; break;     /* 打招呼 */
		case 14: frames = stretch; frame_count = 3; break;                            /* 伸懒腰 */
		case 15: frames = leg_stretch; frame_count = 4; break;                        /* 后腿拉伸 */
	}
}

/**
  * @brief  动作系统初始化：标记空闲并请求站立姿态
  */
void PetAction_Init(void)
{
	active_action = 0xFF;
	PetAction_Request(2, 0);
}

/**
  * @brief  请求执行某个动作（可随时打断当前动作）
  * @param  action    动作编号 0~15（9 保留）
  * @param  sustained 1=持续执行（蓝牙移动命令），0=按重复次数执行
  * @note   立即把第一帧角度作为目标交给舵机，后续帧由 Task20ms 推进
  */
void PetAction_Request(uint8_t action, uint8_t sustained)
{
	if (action > 15 || action == 9) return;   /* 非法编号直接忽略 */
	Action_Mode = action;                     /* 同步全局动作标志（遥测用） */
	Sustainedmove = sustained;
	active_action = action;
	sustained_action = sustained;
	frame_index = 0;
	frame_elapsed = 0;
	SelectAction(action);
	Servo_SetTargets(frames[0].angle, FrameDuration());   /* 先向第一帧目标运动 */
}

/**
  * @brief  紧急停止：回到站立姿态（协议 0x62 使用）
  */
void PetAction_Stop(void)
{
	PetAction_Request(2, 0);
	Servo_SetTargets(pose_stand[0].angle, 100);
}

/**
  * @brief  查询是否正在执行动作
  * @retval 1=动作进行中；0=空闲或正处于站立姿态
  */
uint8_t PetAction_IsBusy(void)
{
	return active_action != 0xFF && active_action != 2;
}

/**
  * @brief  动作 20ms 周期任务（主循环每 20ms 调用一次）
  * @note   流程：
  *         1. 先推进舵机插值 Servo_Task20ms()；
  *         2. 累计当前帧播放时间，满帧长后切下一帧；
  *         3. 全部帧播完后：持续命令/还有重复次数 -> 循环重播；
  *            否则静态姿态动作置为空闲，动态动作请求回站立
  */
void PetAction_Task20ms(void)
{
	uint16_t duration;
	Servo_Task20ms();                        /* 舵机向当前帧目标平滑插值一步 */
	if (active_action == 0xFF) return;       /* 空闲则无事可做 */
	duration = FrameDuration();
	frame_elapsed += 20;
	if (frame_elapsed < duration) return;    /* 当前帧还没播完 */
	frame_elapsed = 0;
	frame_index++;
	if (frame_index >= frame_count)          /* 最后一帧已播完 */
	{
		if (sustained_action || repeat_left > 1)   /* 持续命令或还有重复次数：循环重播 */
		{
			if (!sustained_action) repeat_left--;
			frame_index = 0;
		}
		else
		{
			/* 静态姿态（0~3）结束：保持姿态但标记空闲，表情恢复普通眼睛 */
			if (active_action <= 3)
			{
				if (active_action != 0) Face_Mode = 5;
				active_action = 0xFF;
				return;
			}
			/* 动态动作结束：表情恢复普通眼睛并请求回站立姿态 */
			Face_Mode = 5;
			PetAction_Request(2, 0);
			return;
		}
	}
	Servo_SetTargets(frames[frame_index].angle, FrameDuration());   /* 切换到下一帧目标 */
}

/**
  * @brief  兼容旧版入口：若外部直接修改了 Action_Mode，则按新值请求动作
  * @note   新版代码中命令统一走 PetState_Command -> PetAction_Request，
  *         此函数保留用于兼容旧的中断直接改标志位的用法
  */
void PetAction_Perform(void)
{
	if (Action_Mode != active_action && Action_Mode != 9) PetAction_Request((uint8_t)Action_Mode, (uint8_t)Sustainedmove);
}
