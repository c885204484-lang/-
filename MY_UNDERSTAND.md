#一开始进入main()

##GetBattery_Init();//电量检测初始化->实际就是进行AD初始化->AD_Init();->配置PA4为模拟输入

##Servo_Init();//舵机初始化->PWM_Init();->配置TIM2和3->使用了PA0,1,2,3,6，PB0,1分别对应TIM2的通道1,2,3,4和TIM3的通道1，3,4->配置TIM2是为了用PWM控制5个舵机，配置TIM3是为了用PWM控制呼吸灯->预分频器（PSC）都为72-1，自动重装载寄存器（ARR）都为20000-1，时钟频率为72MHz，故PWM周期为0.02s也就是20ms->开启TIM3中断（20ms进一次）

##OLED_Init();//OLED初始化

##BlueTooth_Init();//蓝牙初始化->语音模块使用USART1，故配置PA9,10。蓝牙模块使用USART3，故配置PB10,11->USART的波特率9600，不需要奇偶校验，停止位为1，字长8位，USART3的波特率为115200，其他一样->USART1,3都配置中断，也就是说接收到语音或蓝牙信息就中断处理

##OLED_ShowImage(0,0,128,64,Face_eyes);OLED_Update();显示初始化表情

###进入到while(1)

##PetAction_Perform();动作判断，接受语音或蓝牙信号的指令进行控制
->使用Action_Mode作为标志位来判断当前的状态，而Action_Mode的值是由USART1,3的中断来指定的，也就是说可以用语音或者蓝牙直接指定Action_Mode的值，进而做出相应的动作，动作由相应的舵机来控制，函数为Action_XXX(PetAction.c)。eg：在接受到语音信号后，开始USART1_IRQHandler()中断，如果接受到的指令为0x31，那就是Action_Mode=2。Action_Mode=2的意思就是正常站立，由舵机来驱动，函数为Action_upright()，具体内容为：Servo_Angle1(90)；Servo_Angle2(90);Delay_ms(80);Servo_Angle3(90);Servo_Angle4(90);Servo_AngleX(Angle)实际上就是先将输入的Angle转化为CCR的值，然后再通过TIM_SetCompareX来设置CCR的值，进而利用PWM进行驱动舵机
->初始化的Action_Mode==2，也就是站立模式

##Face_Config();表情判断，接受语音或蓝牙信号的指令进行控制
->使用Face_Mode作为标志位来判断当前的表情，而Face_Mode的值是由USART1,3的中断来指定的，也就是说可以用语音或者蓝牙直接指定Face_Mode的值，进而做出相应的表情，表情由OLED来显示，函数为OLED_ShowImage()，显示的表情由OLED_Data.c来定义。eg：在接受到语音信号后，开始USART1_IRQHandler()中断，如果接受到的指令为0x31，那就是Face_Mode=5。Face_Mode=5的意思就是正常的显示眼睛
->初始化的Face_Mode==2，也就是正常眼睛

###进入到TIM3_IRQHandler//每20ms进一次这个中断

##GetCur_Power();电量检测
->利用GetCur_Power()函数来读取100次的电量值，最后取平均后输出到Face_Config()进行电量显示
->每一次电量值由GetBattery(void)来提供，具体来说是输出Battery_Value=(3.3*4*Get_ADC()/4096)*100-300，如果电池最大电压为4.2V的话，Battery_Value的取值范围为[0,120]。如果Battery_Value的值＞110的话，OLED显示“在充电”，在此以下都显示相应的电量数字
->利用Get_ADC()来获取PA4转换的0~4095/4数字量

##LED_Breathing();呼吸灯
->if(AllLed==1 && BreatheLed==0)//如果灯光开启且不开启呼吸灯,亮度拉满，初始化AllLed==1，BreatheLed==0
->else if(AllLed==1 && BreatheLed==1)//如果灯光开启且开启呼吸灯，初始化后panduan==1，表示灯渐亮，直到4s后达到最亮后，切换成panduan==2，灯渐灭，直到4s后完全灭，切换panduan==3，相当于延时0.4s，什么也不做，结束后切换到panduan==1，以此循环
->如果AllLed==0，表示灯全灭


中断优先级：TIM3(电量检测和呼吸灯) > USART1(语音) > USART3(蓝牙)






