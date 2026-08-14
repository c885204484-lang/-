/**
  ******************************************************************************
  * @file    Face_Config.c
  * @brief   OLED 表情与电量显示：只在内容变化时才重绘整屏（按需刷新，省时省电）
  *
  * 【表情编号】(Face_Mode，与旧版协议兼容)
  *   0 睡觉   1 瞪大眼   2 快乐   3 狂热
  *   4 非常快乐 5 普通眼睛(默认) 6 打招呼
  *
  * 【两种显示模式】
  *   1. 状态页模式(Status_Display_Bit=1，协议 0x60 触发，5秒后自动退出)：
  *      全屏显示 心情/精力/活跃度/电量；
  *   2. 正常模式：全屏显示表情图案 + 可选的电量条/电量数字。
  *
  * 【按需刷新机制】记录上次的表情/电量/开关状态，全部未变化则直接返回，
  *   避免每 200ms 重复向 OLED 发送整屏数据。
  * 【热插拔支持】每约 2 秒(10次调用)重发一次 OLED 配置命令，
  *   保证运行中插拔 OLED 也能恢复显示。
  ******************************************************************************
  */
#include "stm32f10x.h"      
#include "OLED.h"
#include "BlueTooth.h"
#include "Variable.h"
#include "PetState.h"

/**
  * @brief  在屏幕右上角绘制电量图标（电池框 + 格数）
  * @param  percent 电量百分比 0~100，每 20% 亮一格，最多 5 格
  */
static void ShowBattery(uint8_t percent)
{
	uint8_t level = (uint8_t)(percent / 20);
	uint8_t i;
	if (level > 5) level = 5;
	OLED_ClearArea(94, 0, 34, 10);                       /* 清除图标区域 */
	OLED_DrawRectangle(96, 0, 20, 8, OLED_UNFILLED);     /* 电池外壳（空心矩形） */
	OLED_DrawRectangle(116, 2, 2, 4, OLED_FILLED);       /* 电池正极凸起 */
	for (i = 0; i < 5; i++)
	{
		if (i < level) OLED_DrawRectangle(98 + i * 3, 2, 2, 4, OLED_FILLED);   /* 按电量点亮格数 */
	}
}

/**
  * @brief  表情与电量显示周期任务（清醒时每 200ms，睡眠时每 1000ms 调用）
  * @note   内部有静态"上次状态"记录，内容未变化时立即返回（按需刷新）
  */
void Face_Config(void)
{
	static uint16_t last_face = 0xFFFF;        /* 上次的表情编号（初始0xFFFF保证首次必刷新） */
	static uint16_t last_battery = 0xFFFF;     /* 上次的电量值 */
	static uint8_t last_battery_bit = 0xFF;    /* 上次的电量显示开关 */
	static uint8_t last_status = 0xFF;         /* 上次的状态页开关 */
	static uint8_t last_charging = 0xFF;       /* 上次的充电标志 */
	static uint8_t wakeup_counter = 0;         /* 热插拔唤醒计数 */
	const PetStatus *pet = PetState_Get();

	/* 每2秒左右重发一次OLED配置命令，支持热插拔OLED */
	if (++wakeup_counter >= 10)
	{
		wakeup_counter = 0;
		OLED_Wakeup();
	}

	/* 按需刷新：所有显示相关状态都未变化 -> 直接返回，不重发整屏 */
	if (last_face == Face_Mode && last_battery == CurBattery && last_battery_bit == Battery_Bit && last_status == Status_Display_Bit && last_charging == Battery_Charging) return;
	/* 记录本次状态 */
	last_face = Face_Mode;
	last_battery = CurBattery;
	last_battery_bit = Battery_Bit;
	last_status = Status_Display_Bit;
	last_charging = Battery_Charging;

	/* ---- 模式一：状态显示页（0x60 查询触发，5秒后自动退出） ---- */
	if (Status_Display_Bit)
	{
		OLED_Clear();
		OLED_ShowString(0, 0, "STATUS", OLED_6X8);
		OLED_ShowString(0, 16, "MOOD:", OLED_6X8); OLED_ShowNum(36, 16, pet->mood, 3, OLED_6X8); OLED_ShowString(54, 16, "%", OLED_6X8);
		OLED_ShowString(0, 32, "ENERGY:", OLED_6X8); OLED_ShowNum(48, 32, pet->energy, 3, OLED_6X8); OLED_ShowString(66, 32, "%", OLED_6X8);
		OLED_ShowString(0, 48, "ACTIVE:", OLED_6X8); OLED_ShowNum(48, 48, pet->activity, 3, OLED_6X8); OLED_ShowString(66, 48, "%", OLED_6X8);
		ShowBattery((uint8_t)CurBattery);
		if (Battery_Charging) OLED_ShowString(72, 0, "CHG", OLED_6X8);   /* 充电中 */
		else OLED_ShowNum(72, 0, CurBattery, 3, OLED_6X8);               /* 显示电量数字 */
		OLED_ShowString(98, 16, "BAT", OLED_6X8);
		OLED_Update();
		return;
	}

	/* ---- 模式二：正常表情显示 ---- */
	/*图案处理*/
	switch(Face_Mode)
	{
		case 0:
	    	OLED_Clear();
	    	OLED_ShowImage(0,0,128,64,Face_sleep);//睡觉
		  	break;
		case 1:
			OLED_Clear();
		  	OLED_ShowImage(0,0,128,64,Face_stare);//瞪大眼
		  	break;
		case 2:
			OLED_Clear();
	  		OLED_ShowImage(0,0,128,64,Face_happy);//快乐
		  	break;
	    case 3:
			OLED_Clear();
	  		OLED_ShowImage(0,0,128,64,Face_mania);//狂热
		 	break;
		case 4:
			OLED_Clear();
	  		OLED_ShowImage(0,0,128,64,Face_very_happy);//非常快乐
		  	break;
		case 5:
			OLED_Clear();
	  		OLED_ShowImage(0,0,128,64,Face_eyes);//眼睛
		 	break;
		case 6:
			OLED_Clear();
	  		OLED_ShowImage(0,0,128,64,Face_hello);//打招呼
			break;
	}
	
	/*电量处理：Battery_Bit==1 时在屏幕顶部叠加电量信息*/
	if(Battery_Bit)//Battery_Bit==1
	{
		OLED_ClearArea(0, 0, 128, 10);                     /* 清除顶部 10 像素行 */
		if (Battery_Charging) OLED_ShowString(0,0,"Charging",OLED_6X8);   /* 充电中 */
		else
		{
			OLED_ShowString(0,0,"Power:",OLED_6X8);
			OLED_ShowNum(36,0,CurBattery,3,OLED_6X8);
			OLED_ShowString(54,0,"%",OLED_6X8);
		}
		ShowBattery((uint8_t)CurBattery);
	}
	
	/*显示图案：把显存数据一次性发送到 OLED 硬件*/
	OLED_Update();
}
