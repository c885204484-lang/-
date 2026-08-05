#ifndef __PET_STATE_H
#define __PET_STATE_H

#include "stm32f10x.h"

typedef enum
{
	PET_HEALTH_OK = 0,
	PET_HEALTH_LOW_BATTERY = 1,
	PET_HEALTH_EXHAUSTED = 2
} PetHealth;

typedef struct
{
	uint8_t mood;
	uint8_t energy;
	uint8_t activity;
	uint8_t sleeping;
	PetHealth health;
	uint32_t last_interaction_ms;
	uint32_t uptime_seconds;
} PetStatus;

void PetState_Init(void);
void PetState_Task1s(void);
void PetState_Task100ms(void);
uint8_t PetState_Command(uint8_t command, uint8_t sustained);
const PetStatus *PetState_Get(void);

#endif
