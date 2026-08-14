/**
  ******************************************************************************
  * @file    AD.c
  * @brief   ADC1 电池电压采样：TIM3 定时触发 + DMA1 循环搬运，两级滤波的第一步
  *
  * 【采样链路】
  *   TIM3 每 20ms 产生更新事件 -> TRGO 触发 ADC1_CH4(PA4) 转换一次
  *   -> 转换结果由 DMA1_Channel1 自动写入 32 点循环缓冲 adc_samples[]
  *   -> 主循环每 500ms 调用 AD_GetAverage() 求 32 点平均
  *   -> PowerDetection.c 再做 8 次滑动平均，得到稳定的电量值
  *
  * 【优点】ADC 采样与 DMA 搬运全部由硬件完成，不占用 CPU 与中断
  ******************************************************************************
  */
#include "stm32f10x.h"
#include "AD.h"

#define ADC_DMA_SAMPLES 32    /* DMA 循环缓冲长度：32 个采样点 */

static volatile uint16_t adc_samples[ADC_DMA_SAMPLES];   /* DMA 采样缓冲（volatile：DMA 异步写入） */

/**
  * @brief  ADC1 + DMA1 初始化
  * @note   配置步骤：GPIO -> DMA -> ADC 结构体 -> 校准 -> 使能外部触发
  */
void AD_Init(void)
{
	GPIO_InitTypeDef gpio;
	ADC_InitTypeDef adc;
	DMA_InitTypeDef dma;

	/* 开启 ADC1、GPIOA 时钟；DMA1 挂在 AHB 总线上 */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1 | RCC_APB2Periph_GPIOA, ENABLE);
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
	RCC_ADCCLKConfig(RCC_PCLK2_Div6);   /* ADC 时钟 = 72MHz/6 = 12MHz（ADC 最高允许 14MHz） */

	/* PA4 配置为模拟输入（电池电压经分压电路接入） */
	gpio.GPIO_Mode = GPIO_Mode_AIN;
	gpio.GPIO_Pin = GPIO_Pin_4;
	gpio.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &gpio);

	/* ---- DMA1_Channel1 配置：外设(ADC数据寄存器) -> 内存(adc_samples)，循环模式 ---- */
	DMA_DeInit(DMA1_Channel1);
	dma.DMA_PeripheralBaseAddr = (uint32_t)&ADC1->DR;              /* 源：ADC1 数据寄存器 */
	dma.DMA_MemoryBaseAddr = (uint32_t)adc_samples;                /* 目的：采样缓冲 */
	dma.DMA_DIR = DMA_DIR_PeripheralSRC;                           /* 外设 -> 内存 */
	dma.DMA_BufferSize = ADC_DMA_SAMPLES;                          /* 一次传输 32 个半字 */
	dma.DMA_PeripheralInc = DMA_PeripheralInc_Disable;             /* 外设地址不递增 */
	dma.DMA_MemoryInc = DMA_MemoryInc_Enable;                      /* 内存地址递增 */
	dma.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;  /* 12位ADC结果用16位存储 */
	dma.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
	dma.DMA_Mode = DMA_Mode_Circular;                              /* 循环模式：写满32点后从头覆盖 */
	dma.DMA_Priority = DMA_Priority_Medium;
	dma.DMA_M2M = DMA_M2M_Disable;                                 /* 内存到内存模式关闭 */
	DMA_Init(DMA1_Channel1, &dma);
	DMA_Cmd(DMA1_Channel1, ENABLE);                                /* 使能 DMA */

	/* ---- ADC1 配置 ---- */
	adc.ADC_Mode = ADC_Mode_Independent;                           /* 独立模式（不使用双ADC） */
	adc.ADC_ScanConvMode = DISABLE;                                /* 非扫描模式（单通道） */
	adc.ADC_ContinuousConvMode = DISABLE;                          /* 非连续转换：由外部触发启动 */
	adc.ADC_ExternalTrigConv = ADC_ExternalTrigConv_T3_TRGO;       /* 外部触发源：TIM3 的 TRGO（每20ms） */
	adc.ADC_DataAlign = ADC_DataAlign_Right;                       /* 右对齐 */
	adc.ADC_NbrOfChannel = 1;                                      /* 转换通道数 1 */
	ADC_Init(ADC1, &adc);
	ADC_RegularChannelConfig(ADC1, ADC_Channel_4, 1, ADC_SampleTime_239Cycles5);  /* 规则组通道4(PA4)，最长采样时间 */
	ADC_DMACmd(ADC1, ENABLE);                                      /* 使能 ADC 的 DMA 请求 */
	ADC_Cmd(ADC1, ENABLE);                                         /* 使能 ADC */

	/* ---- ADC 校准（上电后必须执行一次） ---- */
	ADC_ResetCalibration(ADC1);
	while (ADC_GetResetCalibrationStatus(ADC1));
	ADC_StartCalibration(ADC1);
	while (ADC_GetCalibrationStatus(ADC1));

	ADC_ExternalTrigConvCmd(ADC1, ENABLE);   /* 使能外部触发转换（此后每 20ms 自动采样一次） */
}

/**
  * @brief  读取 DMA 缓冲中 32 个采样点的平均值
  * @retval 平均后的 12 位 ADC 原始值（0~4095）
  */
uint16_t AD_GetAverage(void)
{
	uint8_t i;
	uint32_t sum = 0;
	for (i = 0; i < ADC_DMA_SAMPLES; i++) sum += adc_samples[i];
	return (uint16_t)(sum / ADC_DMA_SAMPLES);
}

/**
  * @brief  兼容旧版接口：获取 ADC 平均值
  */
uint16_t Get_ADC(void)
{
	return AD_GetAverage();
}
