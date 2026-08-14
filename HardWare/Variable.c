/**
  ******************************************************************************
  * @file    Variable.c
  * @brief   全局运行参数定义（兼容原项目的全局变量风格，外部通过 Variable.h 引用）
  *
  * @note    新版代码中动作/表情/状态已收敛到 PetAction/PetState 模块，
  *          这里保留全局变量用于兼容旧版中断直改标志位的用法以及遥测上报。
  ******************************************************************************
  */
/*在这里定义所有的变量，并允许外部引用*/
#include "stm32f10x.h"                  // Device header
#include "Variable.h"


/*动作相关*/
uint16_t PAnumbers=Chongfunumber;//动作重复次数
uint16_t TiaoTurn=0;             //向前跳
uint16_t TiaoTurn2=0;            //向后跳
uint16_t Action_Mode=2;   //动作模式,上电后是站立状态
uint16_t SpeedDelay=200;  //运动速度(ms/帧)，0x38/0x63 可调
uint16_t SwingDelay=6;    //摇摆速度
uint16_t Face_Mode=5;     //表情切换，上电是两个眼睛表情
uint8_t WeiBa_Bit=0;      //摇尾巴判断(0x40 开关)
uint8_t WeiBa_Value=90;   //摇尾巴的compare值
int8_t WeiBa_Dir=1;       //摇尾巴方向判断
uint16_t Sustainedmove=0; //持续运动(蓝牙通道移动命令)


/*呼吸灯相关*/
uint16_t Time;            //呼吸灯间隔时间
uint16_t HuXi;            //呼吸灯输出脉冲数(当前亮度)
uint16_t PanDuan=1;       //呼吸灯模式(1渐亮/2渐灭/3停顿)
uint16_t Wait=0;          //间隔时间(停顿阶段计时)
uint16_t AllLed=1;        //开启灯光(0x44/0x45 控制)
uint16_t BreatheLed=0;    //呼吸灯，BreatheLed=0表示关闭呼吸灯

/*测电量相关*/
float Battery_Value=0;    //当前电量百分比(0~100)
uint16_t temp;            //每20ms测得的当前电量(单次采样值)
uint16_t temp1=0;         //0-100次电量累加值
uint16_t CurBattery;      //当前电量(滤波后的最终值，状态机/OLED/遥测使用)
uint8_t Battery_num=0;    //获取电量滤波系数
uint8_t Battery_Bit=1;    //OLED 电量显示开关(0x50 切换)
uint8_t Battery_Charging=0;   //充电中标志(电压换算结果判定)
uint8_t Status_Display_Bit=0;     //状态显示页开关(0x60 查询触发)
uint8_t Status_Display_Seconds=0; //状态页剩余显示秒数(5秒后自动退出)
