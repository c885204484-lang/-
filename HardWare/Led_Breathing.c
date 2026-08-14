/**
  ******************************************************************************
  * @file    Led_Breathing.c
  * @brief   LED 控制：常亮 / 呼吸灯 / 关闭 三种模式（由主循环每 100ms 调用）
  *
  * 【模式切换条件】（全局变量由协议命令设置，见 PetState.c）
  *   AllLed==1 && BreatheLed==0 : 常亮（占空比拉满 20000）
  *   AllLed==1 && BreatheLed==1 : 呼吸灯（三段式循环）
  *   AllLed==0                  : 关闭（占空比 0）
  *
  * 【呼吸灯三段式循环】(PanDuan 状态机，每 100ms 推进一次)
  *   PanDuan=1 渐亮：HuXi 每步 +100，20000 步进后（约4s）切 PanDuan=2
  *   PanDuan=2 渐灭：HuXi 每步 -100，减到 0 后切 PanDuan=3
  *   PanDuan=3 停顿：Wait 累加 1000，累计 20000（约2s）后回到 PanDuan=1
  *
  * @note  LED 的 GPIO 与 PWM 通道已在 PWM.c 中配置（PB0=LED1, PB1=LED2）
  ******************************************************************************
  */
#include "stm32f10x.h"                  // Device header
#include "Variable.h"
#include "PWM.h"

/*注意LED的GPIO与PWM配置已经在PWM.c中配置好了，这个函数是在中断中执行的函数*/
void LED_Breathing(void)
{
		if(AllLed==1 && BreatheLed==0)//如果灯光开启且不开启呼吸灯,亮度拉满
		{
			PWM_LED1(20000);   /* 占空比 100%：常亮 */
			PWM_LED2(20000);
		}
		else if(AllLed==1 && BreatheLed==1)//如果灯光开启且开启呼吸灯
		{
			if(PanDuan==1)
			{
				/*中断20ms一次，4s切换到PanDuan2模式*/
				HuXi+=100;              /* 渐亮：每步 +100 */
				PWM_LED1(HuXi);
				PWM_LED2(HuXi);
				if(HuXi==20000)PanDuan=2;   /* 达到最亮 -> 进入渐灭阶段 */
			}
			else if(PanDuan==2)
			{
				/*中断20ms一次，4s切换到PanDuan3模式*/
				HuXi-=100;              /* 渐灭：每步 -100 */
				PWM_LED1(HuXi);
				PWM_LED2(HuXi);
				if(HuXi==0)PanDuan=3;	/* 完全熄灭 -> 进入停顿阶段 */
			}
			else if(PanDuan==3)
			{
				/*中断20ms一次，0.4s切换到PanDuan1模式*/
				Wait+=1000;             /* 停顿计时 */
				if(Wait==20000)
				{
					PanDuan=1;          /* 停顿结束 -> 重新渐亮 */
					Wait=0;
				}
			}
		}
		else if(AllLed==0)
		{
			PWM_LED1(0);    /* 全灭 */
			PWM_LED2(0);
		}
}
