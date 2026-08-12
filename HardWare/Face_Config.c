#include "stm32f10x.h"      
#include "OLED.h"
#include "BlueTooth.h"
#include "Variable.h"
#include "PetState.h"

static void ShowBattery(uint8_t percent)
{
	uint8_t level = (uint8_t)(percent / 20);
	uint8_t i;
	if (level > 5) level = 5;
	OLED_ClearArea(94, 0, 34, 10);
	OLED_DrawRectangle(96, 0, 20, 8, OLED_UNFILLED);
	OLED_DrawRectangle(116, 2, 2, 4, OLED_FILLED);
	for (i = 0; i < 5; i++)
	{
		if (i < level) OLED_DrawRectangle(98 + i * 3, 2, 2, 4, OLED_FILLED);
	}
}

//实现表情变化，调节是进中断后
void Face_Config(void)
{
	static uint16_t last_face = 0xFFFF;
	static uint16_t last_battery = 0xFFFF;
	static uint8_t last_battery_bit = 0xFF;
	static uint8_t last_status = 0xFF;
	static uint8_t last_charging = 0xFF;
	static uint8_t wakeup_counter = 0;
	const PetStatus *pet = PetState_Get();

	/* 每2秒左右重发一次OLED配置命令，支持热插拔OLED */
	if (++wakeup_counter >= 10)
	{
		wakeup_counter = 0;
		OLED_Wakeup();
	}

	if (last_face == Face_Mode && last_battery == CurBattery && last_battery_bit == Battery_Bit && last_status == Status_Display_Bit && last_charging == Battery_Charging) return;
	last_face = Face_Mode;
	last_battery = CurBattery;
	last_battery_bit = Battery_Bit;
	last_status = Status_Display_Bit;
	last_charging = Battery_Charging;
	if (Status_Display_Bit)
	{
		OLED_Clear();
		OLED_ShowString(0, 0, "STATUS", OLED_6X8);
		OLED_ShowString(0, 16, "MOOD:", OLED_6X8); OLED_ShowNum(36, 16, pet->mood, 3, OLED_6X8); OLED_ShowString(54, 16, "%", OLED_6X8);
		OLED_ShowString(0, 32, "ENERGY:", OLED_6X8); OLED_ShowNum(48, 32, pet->energy, 3, OLED_6X8); OLED_ShowString(66, 32, "%", OLED_6X8);
		OLED_ShowString(0, 48, "ACTIVE:", OLED_6X8); OLED_ShowNum(48, 48, pet->activity, 3, OLED_6X8); OLED_ShowString(66, 48, "%", OLED_6X8);
		ShowBattery((uint8_t)CurBattery);
		if (Battery_Charging) OLED_ShowString(72, 0, "CHG", OLED_6X8);
		else OLED_ShowNum(72, 0, CurBattery, 3, OLED_6X8);
		OLED_ShowString(98, 16, "BAT", OLED_6X8);
		OLED_Update();
		return;
	}
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
	
	/*电量处理*/
	if(Battery_Bit)//Battery_Bit==1
	{
		OLED_ClearArea(0, 0, 128, 10);
		if (Battery_Charging) OLED_ShowString(0,0,"Charging",OLED_6X8);
		else
		{
			OLED_ShowString(0,0,"Power:",OLED_6X8);
			OLED_ShowNum(36,0,CurBattery,3,OLED_6X8);
			OLED_ShowString(54,0,"%",OLED_6X8);
		}
		ShowBattery((uint8_t)CurBattery);
	}
	
	/*显示图案*/
		OLED_Update();
}
