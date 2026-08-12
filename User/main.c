#include "stm32f10x.h"
#include "OLED.h"
#include "BlueTooth.h"
#include "Servo.h"
#include "PetAction.h"
#include "PetState.h"
#include "Face_Config.h"
#include "PowerDetection.h"
#include "Led_Breathing.h"
#include "Scheduler.h"
#include "ConfigStore.h"
#include "Watchdog.h"

int main(void)
{
	uint32_t task20 = 0;
	uint32_t task100 = 0;
	uint32_t task200 = 0;
	uint32_t task500 = 0;
	uint32_t task1000 = 0;

	ConfigStore_Load();
	GetBattery_Init();
	Servo_Init();
	OLED_Init();
	BlueTooth_Init();
	Scheduler_Init();
	PetState_Init();
	PetAction_Init();
	Watchdog_Init();
	Face_Config();

	while (1)
	{
		const PetStatus *pet = PetState_Get();
		BlueTooth_Task();
		if (Scheduler_Elapsed(&task20, 20)) PetAction_Task20ms();
		if (Scheduler_Elapsed(&task100, 100))
		{
			PetState_Task100ms();
			LED_Breathing();
		}
		if (Scheduler_Elapsed(&task200, pet->sleeping ? 1000 : 200)) Face_Config();
		if (Scheduler_Elapsed(&task500, 500)) GetCur_Power();
		if (Scheduler_Elapsed(&task1000, pet->sleeping ? 5000 : 1000))
		{
			PetState_Task1s();
			BlueTooth_SendStatus();
		}

		Watchdog_Feed();
		__WFI();
	}
}
