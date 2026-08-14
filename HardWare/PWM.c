/**
  ******************************************************************************
  * @file    PWM.c
  * @brief   PWM 初始化与输出函数：TIM2 驱动 4 路腿部舵机，TIM3 驱动尾巴舵机和 2 路 LED
  *
  * 【硬件资源】
  *   TIM2_CH1~CH4 -> PA0/PA1/PA2/PA3：4 路腿部舵机
  *   TIM3_CH1     -> PA6         ：尾巴舵机
  *   TIM3_CH3/CH4 -> PB0/PB1     ：LED1/LED2（呼吸灯调光）
  *
  * 【时序参数】
  *   预分频 PSC = 72-1，自动重装 ARR = 20000-1，72MHz 时钟下：
  *   PWM 频率 = 72MHz / 72 / 20000 = 50Hz，周期 20ms（舵机标准周期）
  *
  * 【ADC 触发】TIM3 更新事件通过 TRGO 输出，作为 ADC1 的外部触发源
  *   （见 AD.c），实现每 20ms 自动采样一次电池电压。
  ******************************************************************************
  */
#include "stm32f10x.h"                  // Device header

/**
  * @brief  PWM 初始化：配置 TIM2/TIM3 及相关 GPIO 为 PWM 输出
  * @note   本函数只初始化硬件和输出比较通道，周期任务由 SysTick 调度器驱动
  */
void PWM_Init(void)
{
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2,ENABLE);//开启TIM2时钟
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3,ENABLE);//开启TIM3时钟
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA,ENABLE);//开启GPIOA时钟
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB,ENABLE);//开启GPIOB时钟
	//配置TIM2是为了用PWM控制5个舵机
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_AF_PP;//复用推挽输出模式
	GPIO_InitStructure.GPIO_Pin=GPIO_Pin_0|GPIO_Pin_1|GPIO_Pin_2|GPIO_Pin_3|GPIO_Pin_6;//默认PA0是TIM2通道1的复用，PA1是TIM2通道2的复用所以开启这俩IO口...
	GPIO_InitStructure.GPIO_Speed=GPIO_Speed_50MHz;
	GPIO_Init(GPIOA,&GPIO_InitStructure);
	//配置TIM3是为了用PWM控制呼吸灯
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_AF_PP;//复用推挽输出模式
	GPIO_InitStructure.GPIO_Pin=GPIO_Pin_0|GPIO_Pin_1;
	GPIO_InitStructure.GPIO_Speed=GPIO_Speed_50MHz;
	GPIO_Init(GPIOB,&GPIO_InitStructure);
	
	TIM_InternalClockConfig(TIM2);//TIM2切换为内部定时器
	TIM_InternalClockConfig(TIM3);//TIM3切换为内部定时器
	
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
	TIM_TimeBaseInitStructure.TIM_ClockDivision=TIM_CKD_DIV1;//不分频
	TIM_TimeBaseInitStructure.TIM_CounterMode=TIM_CounterMode_Up;//向上计数
	TIM_TimeBaseInitStructure.TIM_Period=20000-1;   //自动重装载值：决定 PWM 周期为 20ms
	TIM_TimeBaseInitStructure.TIM_Prescaler=72-1;   //预分频：72MHz/72 = 1MHz 计数频率
	TIM_TimeBaseInitStructure.TIM_RepetitionCounter=0;
	TIM_TimeBaseInit(TIM2,&TIM_TimeBaseInitStructure);
	TIM_TimeBaseInit(TIM3,&TIM_TimeBaseInitStructure);
	TIM_SelectOutputTrigger(TIM3, TIM_TRGOSource_Update);  //TIM3 更新事件输出到 TRGO，触发 ADC 采样
	
	/* Periodic work is scheduled by the 1 ms SysTick scheduler. */
	/*（周期任务由 1ms SysTick 调度器驱动，无需 TIM3 中断）*/
	
	TIM_OCInitTypeDef TIM_OCInitStructure;
	TIM_OCStructInit(&TIM_OCInitStructure);
	TIM_OCInitStructure.TIM_OCMode=TIM_OCMode_PWM1;//输出比较模式采用PWM1
	TIM_OCInitStructure.TIM_OCPolarity=TIM_OCPolarity_High;
	TIM_OCInitStructure.TIM_OutputState=TIM_OutputState_Enable;
	TIM_OCInitStructure.TIM_Pulse=0;//初始化CCR的值为0
	TIM_OC1Init(TIM2,&TIM_OCInitStructure);//TIM2复用通道1开启（舵机1 左前腿）
	TIM_OC2Init(TIM2,&TIM_OCInitStructure);//TIM2复用通道2开启（舵机2 右前腿）
	TIM_OC3Init(TIM2,&TIM_OCInitStructure);//TIM2复用通道3开启（舵机3 左后腿）
	TIM_OC4Init(TIM2,&TIM_OCInitStructure);//TIM2复用通道4开启（舵机4 右后腿）
	
	TIM_OC1Init(TIM3,&TIM_OCInitStructure);//TIM3复用通道1开启（尾巴舵机）
	TIM_OC3Init(TIM3,&TIM_OCInitStructure);//TIM3复用通道3开启（LED1）
	TIM_OC4Init(TIM3,&TIM_OCInitStructure);//TIM3复用通道4开启（LED2）
	
	TIM_Cmd(TIM2,ENABLE);//使能TIM2
	TIM_Cmd(TIM3,ENABLE);//使能TIM3
}

/**
  * @brief  设置 TIM2_CH1 的占空比（舵机1 左前腿）
  * @param  Compare 比较值 CCR，范围 0~20000（对应脉宽 0~20ms）
  */
void PWM_SetCompare1(uint16_t Compare)
{
	
	TIM_SetCompare1(TIM2, Compare);//设置CCR1的值		
}

/**
  * @brief  设置 TIM2_CH2 的占空比（舵机2 右前腿）
  */
void PWM_SetCompare2(uint16_t Compare)
{
			
	TIM_SetCompare2(TIM2, Compare);//设置CCR2的值
}

/**
  * @brief  设置 TIM2_CH3 的占空比（舵机3 左后腿）
  */
void PWM_SetCompare3(uint16_t Compare)
{
			
	TIM_SetCompare3(TIM2, Compare);//设置CCR3的值
}

/**
  * @brief  设置 TIM2_CH4 的占空比（舵机4 右后腿）
  */
void PWM_SetCompare4(uint16_t Compare)
{
			
	TIM_SetCompare4(TIM2, Compare);//设置CCR4的值
}

/**
  * @brief  设置 TIM3_CH1 的占空比（尾巴舵机）
  */
void PWM_WSetCompare(uint16_t Compare)
{
	TIM_SetCompare1(TIM3, Compare);//设置尾巴CCR1的值
}

/**
  * @brief  设置 TIM3_CH3 的占空比（LED1 亮度）
  */
void PWM_LED1(uint16_t Compare)
{
	TIM_SetCompare3(TIM3,Compare);
}

/**
  * @brief  设置 TIM3_CH4 的占空比（LED2 亮度）
  */
void PWM_LED2(uint16_t Compare)
{
	TIM_SetCompare4(TIM3,Compare);
}
