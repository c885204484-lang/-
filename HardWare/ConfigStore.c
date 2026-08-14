/**
  ******************************************************************************
  * @file    ConfigStore.c
  * @brief   Flash 配置存储：掉电保存速度/灯光/宠物状态/舵机校准，硬件 CRC 校验
  *
  * 【存储布局】使用 STM32F103C8 64KB Flash 的最后一个 1KB 页面：
  *   CONFIG_ADDRESS = 0x0800FC00
  *   StoredConfig = { uint32_t magic; PetConfig config; uint32_t checksum; }
  *   - magic     ：固定魔数 0x50455431("PET1")，用于快速判断页面是否有效
  *   - config    ：业务配置结构体（见 ConfigStore.h 的 PetConfig）
  *   - checksum  ：对 config 用 STM32 硬件 CRC 外设计算的 32 位 CRC
  *
  * 【读取流程】魔数匹配 + CRC 匹配 + 参数范围合法 -> 采用保存值；否则用默认值
  * 【写入流程】擦除整页 -> 按半字(16位)编程写入 -> 重新上锁
  *
  * @warning Flash 有擦写寿命限制(约1万次)，请勿周期性发送 0x61 命令
  * @warning 应用代码不得增长到 0x0800FC00 及之后地址
  ******************************************************************************
  */
#include "stm32f10x.h"
#include "ConfigStore.h"
#include "Variable.h"
#include "Servo.h"
#include "PetState.h"

#define CONFIG_ADDRESS 0x0800FC00UL   /* 配置存储页地址：64KB Flash 最后一页 */
#define CONFIG_MAGIC 0x50455431UL     /* 配置魔数 "PET1" */

/* Flash 中存储的整体结构：魔数 + 配置 + CRC */
typedef struct
{
	uint32_t magic;
	PetConfig config;
	uint32_t checksum;
} StoredConfig;

static PetConfig current_config;   /* 当前生效的配置（RAM 副本） */

/**
  * @brief  用 STM32 硬件 CRC 外设计算配置结构体的 32 位校验值
  * @param  config 待校验的配置指针
  * @retval CRC32 结果
  * @note   配置按 4 字节对齐，按字喂给 CRC 外设
  */
static uint32_t ConfigChecksum(const PetConfig *config)
{
	const uint32_t *words = (const uint32_t *)config;
	uint8_t i;
	uint32_t value;
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_CRC, ENABLE);
	CRC_ResetDR();                                    /* 复位 CRC 数据寄存器 */
	for (i = 0; i < sizeof(PetConfig) / 4; i++) CRC_CalcCRC(words[i]);
	value = CRC_GetCRC();
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_CRC, DISABLE);
	return value;
}

/**
  * @brief  把 current_config 应用到全局运行参数（舵机校准/速度/灯光等）
  */
static void ApplyConfig(void)
{
	uint8_t i;
	SpeedDelay = current_config.speed_delay;              /* 移动速度 */
	SwingDelay = current_config.swing_delay;              /* 摇摆延时 */
	AllLed = current_config.led_enabled;                  /* LED 开关 */
	BreatheLed = current_config.breathe_enabled;          /* 呼吸灯开关 */
	Battery_Bit = current_config.battery_visible;         /* 电量显示开关 */
	/* 5 路舵机校准：最小角/最大角/零点微调 */
	for (i = 0; i < 5; i++) Servo_SetCalibration(i, current_config.servo_min[i], current_config.servo_max[i], current_config.servo_trim[i]);
}

/**
  * @brief  上电加载配置：校验 Flash 中的存储数据，合法则采用，否则用默认值
  * @note   必须在 Servo_Init 之前调用（校准参数影响舵机初始化后的输出）
  */
void ConfigStore_Load(void)
{
	const StoredConfig *stored = (const StoredConfig *)CONFIG_ADDRESS;   /* 直接映射 Flash 地址 */
	uint8_t i;
	/* 三重校验：魔数 + CRC + 关键参数范围 */
	if (stored->magic == CONFIG_MAGIC && stored->checksum == ConfigChecksum(&stored->config) &&
		stored->config.speed_delay >= 80 && stored->config.speed_delay <= 400 &&
		stored->config.swing_delay >= 2 && stored->config.swing_delay <= 20 &&
		stored->config.mood <= 100 && stored->config.energy <= 100)
	{
		current_config = stored->config;   /* 校验通过：采用保存值 */
	}
	else
	{
		/* 校验失败或从未保存：使用安全默认值 */
		current_config.speed_delay = 200;      /* 移动速度 200ms/帧 */
		current_config.swing_delay = 6;        /* 摇摆延时 */
		current_config.led_enabled = 1;        /* LED 默认开启 */
		current_config.breathe_enabled = 0;    /* 呼吸灯默认关闭 */
		current_config.battery_visible = 1;    /* 电量显示默认开启 */
		current_config.mood = 60;              /* 心情默认 60 */
		current_config.energy = 80;            /* 精力默认 80 */
		for (i = 0; i < 5; i++)
		{
			current_config.servo_min[i] = 10;  /* 舵机默认限位 10°~170° */
			current_config.servo_max[i] = 170;
			current_config.servo_trim[i] = 0;  /* 默认无微调 */
		}
		current_config.reserved[0] = current_config.reserved[1] = current_config.reserved[2] = 0;
	}
	ApplyConfig();   /* 应用到全局变量 */
}

/**
  * @brief  保存当前运行参数到 Flash（协议 0x61 触发）
  * @retval 1=保存成功；0=失败（擦除或编程出错）
  * @note   会同步保存宠物当前心情/精力与 5 路舵机校准值
  */
uint8_t ConfigStore_Save(void)
{
	StoredConfig stored;
	uint16_t *words = (uint16_t *)&stored;    /* Flash 按半字编程，按 16 位访问结构体 */
	uint8_t i;
	const PetStatus *pet = PetState_Get();
	FLASH_Status result;
	/* 1. 把当前运行参数收集到 config */
	current_config.speed_delay = SpeedDelay;
	current_config.swing_delay = SwingDelay;
	current_config.led_enabled = (uint8_t)AllLed;
	current_config.breathe_enabled = (uint8_t)BreatheLed;
	current_config.battery_visible = Battery_Bit;
	current_config.mood = pet->mood;
	current_config.energy = pet->energy;
	for (i = 0; i < 5; i++) Servo_GetCalibration(i, &current_config.servo_min[i], &current_config.servo_max[i], &current_config.servo_trim[i]);
	/* 2. 组装整体结构并计算 CRC */
	stored.magic = CONFIG_MAGIC;
	stored.config = current_config;
	stored.checksum = ConfigChecksum(&stored.config);

	/* 3. 解锁 Flash -> 擦除整页 -> 半字编程写入 -> 上锁 */
	FLASH_Unlock();
	result = FLASH_ErasePage(CONFIG_ADDRESS);
	if (result == FLASH_COMPLETE)
	{
		for (i = 0; i < sizeof(StoredConfig) / 2; i++)
		{
			result = FLASH_ProgramHalfWord(CONFIG_ADDRESS + i * 2, words[i]);
			if (result != FLASH_COMPLETE) break;   /* 中途出错立即停止 */
		}
	}
	FLASH_Lock();
	return result == FLASH_COMPLETE;
}

/**
  * @brief  获取当前生效配置的只读指针（PetState_Init 用它恢复心情/精力）
  */
const PetConfig *ConfigStore_Get(void)
{
	return &current_config;
}
