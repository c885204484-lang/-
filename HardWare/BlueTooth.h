#ifndef __BLUE_TOOTH_H
#define __BLUE_TOOTH_H

#include "stm32f10x.h"

void BlueTooth_Init(void);
void BlueTooth_Task(void);
void BlueTooth_SendStatus(void);
uint16_t BlueTooth_GetErrorCount(void);

#endif
