#ifndef __CONFIG_STORE_H
#define __CONFIG_STORE_H

#include "stm32f10x.h"

typedef struct
{
	uint16_t speed_delay;
	uint16_t swing_delay;
	uint8_t led_enabled;
	uint8_t breathe_enabled;
	uint8_t battery_visible;
	uint8_t mood;
	uint8_t energy;
	uint8_t servo_min[5];
	uint8_t servo_max[5];
	int8_t servo_trim[5];
	uint8_t reserved[3];
} PetConfig;

void ConfigStore_Load(void);
uint8_t ConfigStore_Save(void);
const PetConfig *ConfigStore_Get(void);

#endif
