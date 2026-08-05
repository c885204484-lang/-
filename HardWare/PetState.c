#include "stm32f10x.h"
#include "PetState.h"
#include "PetAction.h"
#include "Scheduler.h"
#include "Variable.h"
#include "Servo.h"
#include "ConfigStore.h"

#define SLEEP_TIMEOUT_MS 120000UL
#define LOW_BATTERY_LEVEL 15
#define LOW_ENERGY_LEVEL 15

static PetStatus status;
static uint8_t tail_angle = 90;
static int8_t tail_direction = 1;

static uint8_t IsMotion(uint8_t action)
{
	return action >= 4 && action <= 15 && action != 9;
}

void PetState_Init(void)
{
	const PetConfig *config = ConfigStore_Get();
	status.mood = config->mood;
	status.energy = config->energy;
	status.activity = 50;
	status.sleeping = 0;
	status.health = PET_HEALTH_OK;
	status.last_interaction_ms = Scheduler_Millis();
	status.uptime_seconds = 0;
}

uint8_t PetState_Command(uint8_t command, uint8_t sustained)
{
	uint8_t action = 0xFF;
	status.last_interaction_ms = Scheduler_Millis();
	status.sleeping = 0;
	if (status.mood < 95) status.mood += 5;

	switch (command)
	{
		case 0x29: action = 0; Face_Mode = 0; break;
		case 0x30: action = 1; Face_Mode = 1; break;
		case 0x31: action = 2; Face_Mode = 5; break;
		case 0x32: action = 3; Face_Mode = 1; break;
		case 0x33: action = 4; Face_Mode = 2; break;
		case 0x34: action = 5; Face_Mode = 2; break;
		case 0x35: action = 6; Face_Mode = 2; break;
		case 0x36: action = 7; Face_Mode = 2; break;
		case 0x37: action = 8; Face_Mode = 4; break;
		case 0x38:
			if (SpeedDelay > 100) SpeedDelay -= 20; else SpeedDelay = 200;
			return 1;
		case 0x39:
			if (SwingDelay > 3) SwingDelay--; else SwingDelay = 9;
			return 1;
		case 0x40: WeiBa_Bit ^= 1; return 1;
		case 0x41: action = 10; Face_Mode = 2; break;
		case 0x42: action = 11; Face_Mode = 2; break;
		case 0x43: action = 13; Face_Mode = 6; break;
		case 0x44: AllLed = 1; return 1;
		case 0x45: AllLed = 0; return 1;
		case 0x46: BreatheLed = 1; return 1;
		case 0x47: BreatheLed = 0; return 1;
		case 0x48: action = 14; Face_Mode = 6; break;
		case 0x49: action = 15; Face_Mode = 6; break;
		case 0x50: Battery_Bit ^= 1; return 1;
		default: return 0;
	}

	if ((status.health == PET_HEALTH_LOW_BATTERY || status.energy < LOW_ENERGY_LEVEL) &&
		(action == 10 || action == 11 || action == 8))
	{
		Face_Mode = 1;
		PetAction_Request(1, 0);
		return 0;
	}
	PetAction_Request(action, (uint8_t)(sustained && action >= 4 && action <= 7));
	return 1;
}

void PetState_Task1s(void)
{
	uint32_t idle_ms;
	status.uptime_seconds++;
	idle_ms = Scheduler_Millis() - status.last_interaction_ms;

	if (CurBattery > 0 && CurBattery <= LOW_BATTERY_LEVEL)
	{
		status.health = PET_HEALTH_LOW_BATTERY;
		AllLed = 0;
		Face_Mode = 1;
		if (PetAction_IsBusy()) PetAction_Request(0, 0);
	}
	else if (status.energy < LOW_ENERGY_LEVEL) status.health = PET_HEALTH_EXHAUSTED;
	else status.health = PET_HEALTH_OK;

	if (IsMotion((uint8_t)Action_Mode) && PetAction_IsBusy())
	{
		if (status.energy > 0) status.energy--;
		status.activity = status.energy > 70 ? 90 : 60;
	}
	else
	{
		if (status.energy < 100) status.energy++;
		status.activity = status.sleeping ? 10 : 35;
	}

	if (idle_ms >= SLEEP_TIMEOUT_MS && !status.sleeping)
	{
		status.sleeping = 1;
		Face_Mode = 0;
		BreatheLed = 1;
		PetAction_Request(0, 0);
	}
	if (idle_ms > 30000 && status.mood > 10) status.mood--;
	if (status.mood >= 80 && !status.sleeping && !PetAction_IsBusy())
	{
		WeiBa_Bit = 1;
		Face_Mode = 2;
	}
}

const PetStatus *PetState_Get(void)
{
	return &status;
}

void PetState_Task100ms(void)
{
	uint8_t step;
	if (!WeiBa_Bit || status.sleeping || status.health == PET_HEALTH_LOW_BATTERY) return;
	step = status.activity >= 70 ? 20 : 10;
	if (tail_direction > 0)
	{
		if (tail_angle + step >= 145) { tail_angle = 145; tail_direction = -1; }
		else tail_angle += step;
	}
	else
	{
		if (tail_angle <= 35 + step) { tail_angle = 35; tail_direction = 1; }
		else tail_angle -= step;
	}
	Servo_SetTarget(4, tail_angle, 100);
}
