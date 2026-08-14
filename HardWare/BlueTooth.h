/**
  ******************************************************************************
  * @file    BlueTooth.h
  * @brief   双串口控制模块接口：USART1 语音 + USART3 蓝牙（见 BlueTooth.c 详细说明）
  ******************************************************************************
  */
#ifndef __BLUE_TOOTH_H
#define __BLUE_TOOTH_H

#include "stm32f10x.h"

void BlueTooth_Init(void);                      /* 初始化 USART1(9600)/USART3(115200) 及接收中断 */
void BlueTooth_Task(void);                      /* 主循环周期任务：解析两个环形缓冲区中的命令 */
void BlueTooth_SendStatus(void);                /* 发送 0x70 状态遥测帧（14字节数据区） */
uint16_t BlueTooth_GetErrorCount(void);         /* 获取协议错误+缓冲区溢出总计数 */

#endif
