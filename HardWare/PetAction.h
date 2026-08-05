#ifndef __PET_ACTION_H
#define __PET_ACTION_H

#include "stm32f10x.h"

void PetAction_Init(void);
void PetAction_Request(uint8_t action, uint8_t sustained);
void PetAction_Task20ms(void);
void PetAction_Stop(void);
uint8_t PetAction_IsBusy(void);
void PetAction_Perform(void);

#endif
