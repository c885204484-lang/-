#ifndef __AD_H
#define __AD_H

#include "stm32f10x.h"

void AD_Init(void);
uint16_t AD_GetAverage(void);
uint16_t Get_ADC(void);

#endif
