#ifndef __SCHEDULER_H
#define __SCHEDULER_H

#include "stm32f10x.h"

void Scheduler_Init(void);
uint32_t Scheduler_Millis(void);
uint8_t Scheduler_Elapsed(uint32_t *last, uint32_t period_ms);

#endif
