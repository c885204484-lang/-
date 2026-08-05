#include "stm32f10x.h"
#include "ConfigStore.h"
#include "Variable.h"
#include "Servo.h"
#include "PetState.h"

#define CONFIG_ADDRESS 0x0800FC00UL
#define CONFIG_MAGIC 0x50455431UL

typedef struct
{
	uint32_t magic;
	PetConfig config;
	uint32_t checksum;
} StoredConfig;

static PetConfig current_config;

static uint32_t ConfigChecksum(const PetConfig *config)
{
	const uint32_t *words = (const uint32_t *)config;
	uint8_t i;
	uint32_t value;
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_CRC, ENABLE);
	CRC_ResetDR();
	for (i = 0; i < sizeof(PetConfig) / 4; i++) CRC_CalcCRC(words[i]);
	value = CRC_GetCRC();
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_CRC, DISABLE);
	return value;
}

static void ApplyConfig(void)
{
	uint8_t i;
	SpeedDelay = current_config.speed_delay;
	SwingDelay = current_config.swing_delay;
	AllLed = current_config.led_enabled;
	BreatheLed = current_config.breathe_enabled;
	Battery_Bit = current_config.battery_visible;
	for (i = 0; i < 5; i++) Servo_SetCalibration(i, current_config.servo_min[i], current_config.servo_max[i], current_config.servo_trim[i]);
}

void ConfigStore_Load(void)
{
	const StoredConfig *stored = (const StoredConfig *)CONFIG_ADDRESS;
	uint8_t i;
	if (stored->magic == CONFIG_MAGIC && stored->checksum == ConfigChecksum(&stored->config) &&
		stored->config.speed_delay >= 80 && stored->config.speed_delay <= 400 &&
		stored->config.swing_delay >= 2 && stored->config.swing_delay <= 20 &&
		stored->config.mood <= 100 && stored->config.energy <= 100)
	{
		current_config = stored->config;
	}
	else
	{
		current_config.speed_delay = 200;
		current_config.swing_delay = 6;
		current_config.led_enabled = 1;
		current_config.breathe_enabled = 0;
		current_config.battery_visible = 1;
		current_config.mood = 60;
		current_config.energy = 80;
		for (i = 0; i < 5; i++)
		{
			current_config.servo_min[i] = 10;
			current_config.servo_max[i] = 170;
			current_config.servo_trim[i] = 0;
		}
		current_config.reserved[0] = current_config.reserved[1] = current_config.reserved[2] = 0;
	}
	ApplyConfig();
}

uint8_t ConfigStore_Save(void)
{
	StoredConfig stored;
	uint16_t *words = (uint16_t *)&stored;
	uint8_t i;
	const PetStatus *pet = PetState_Get();
	FLASH_Status result;
	current_config.speed_delay = SpeedDelay;
	current_config.swing_delay = SwingDelay;
	current_config.led_enabled = (uint8_t)AllLed;
	current_config.breathe_enabled = (uint8_t)BreatheLed;
	current_config.battery_visible = Battery_Bit;
	current_config.mood = pet->mood;
	current_config.energy = pet->energy;
	for (i = 0; i < 5; i++) Servo_GetCalibration(i, &current_config.servo_min[i], &current_config.servo_max[i], &current_config.servo_trim[i]);
	stored.magic = CONFIG_MAGIC;
	stored.config = current_config;
	stored.checksum = ConfigChecksum(&stored.config);

	FLASH_Unlock();
	result = FLASH_ErasePage(CONFIG_ADDRESS);
	if (result == FLASH_COMPLETE)
	{
		for (i = 0; i < sizeof(StoredConfig) / 2; i++)
		{
			result = FLASH_ProgramHalfWord(CONFIG_ADDRESS + i * 2, words[i]);
			if (result != FLASH_COMPLETE) break;
		}
	}
	FLASH_Lock();
	return result == FLASH_COMPLETE;
}

const PetConfig *ConfigStore_Get(void)
{
	return &current_config;
}
