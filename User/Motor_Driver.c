#include "Motor_Driver.h"

GPIO_InitTypeDef GPIO_InitStructure;
TIM_TimeBaseInitTypeDef TIM_TIMBaseStructure;
TIM_OCInitTypeDef TIM_OCInitStructure;

void Motor_control(Motor_type motor, float speed){
	uint16_t pwm;
	float magnitude;

	/* 电机限速 -50--50 */
	if(speed > 50.0f) speed = 50.0f;
	else if(speed < -50.0f) speed = -50.0f;

	/* 控制层仍使用0～50，驱动内部映射到CCR 0～999。 */
	magnitude = (speed >= 0.0f) ? speed : -speed;
	pwm = (uint16_t)(magnitude * 999.0f / 50.0f + 0.5f);
	if (pwm > 999U) pwm = 999U;
	/**/
	if(motor == MOTOR_LEFT){
		/* 左轮控制 */
		if(speed > 0.0f){
			/* 正向 */
			GPIO_ResetBits(GPIOB,GPIO_Pin_8);
			GPIO_SetBits(GPIOB,GPIO_Pin_9);
		}
		else if(speed < 0.0f){
			/* 反向 */
			GPIO_ResetBits(GPIOB,GPIO_Pin_9);
			GPIO_SetBits(GPIOB,GPIO_Pin_8);
		}
		else{
			GPIO_ResetBits(GPIOB,GPIO_Pin_9);
			GPIO_ResetBits(GPIOB,GPIO_Pin_8);
		}
		TIM8->CCR4 = pwm;
	}
	else if(motor == MOTOR_RIGHT){
		/* 右轮控制 */
		if(speed > 0.0f){
			/* 正向 */
			GPIO_ResetBits(GPIOD,GPIO_Pin_2);
			GPIO_SetBits(GPIOC,GPIO_Pin_12);
		}
		else if(speed < 0.0f){
			/* 反向 */
			GPIO_SetBits(GPIOD,GPIO_Pin_2);
			GPIO_ResetBits(GPIOC,GPIO_Pin_12);
		}
		else{
			GPIO_ResetBits(GPIOD,GPIO_Pin_2);
			GPIO_ResetBits(GPIOC,GPIO_Pin_12);
		}
		TIM8->CCR1 = pwm;
	}
}


void Motor_init(){
/* 接口配置
 * MotorA PB5 -- AIN1	PB4 -- AIN2		pwm->C7-CCR2
 * MotorB PD2 -- BIN1	PC12 -- BIN2	pwm->C6-CCR1
 * MotorC PB8 -- CIN1	PB9 -- CIN2	    pwm->C9-CCR4
 * MotorD PC14 -- DIN1	PC13 -- DIN2	pwm->C8-CCR3
 */
	//C2 使能
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOC, &GPIO_InitStructure);
	GPIO_SetBits(GPIOC, GPIO_Pin_2);
	//MotorBinit
	MotorB_Init();
	//MotorCinit
	MotorC_Init();
/* TIM8 PWM输出（PC6~PC9是TIM8的固定引脚，不需要重映射）
 * TIM8-CH1 -- PC6	
 * TIM8-CH2 -- PC7
 * TIM8-CH4 -- PC9
 * 
 */
	//TIM8属于APB2高级定时器
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC | RCC_APB2Periph_TIM8, ENABLE);
	//PC6 and 7接收配置
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7 | GPIO_Pin_9;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOC, &GPIO_InitStructure);
/* TIM and PWM
 * TIM8时钟72MHz，PSC=2，ARR=999：PWM频率约24kHz。
 * 1000级占空比分辨率比原来的50级更适合闭环控制。
 * TIM_OCMode_PWM1：选择 PWM Mode 1。计数值小于比较值时，输出有效电平。
 * TIM_OutputState_Enable：允许 Channel 输出。
 * TIM_Pulse：写入比较寄存器 CCR1，决定占空比。
 * TIM_OCPolarity_High：有效电平为高电平。
 * TIM_OC1Init：把配置应用到 TIM3 Channel 1。
 */
	//time base
	TIM_TIMBaseStructure.TIM_Prescaler = 3-1;
	TIM_TIMBaseStructure.TIM_Period = 1000-1;
	TIM_TIMBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TIMBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_TIMBaseStructure.TIM_RepetitionCounter = 0;
	TIM_TimeBaseInit(TIM8, &TIM_TIMBaseStructure);
	
	//OC config
	TIM_OCStructInit(&TIM_OCInitStructure);
	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
	TIM_OCInitStructure.TIM_Pulse = 0;
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
	
	TIM_OC1Init(TIM8, &TIM_OCInitStructure);	//PC6，Motor B PWM
	TIM_OC2Init(TIM8, &TIM_OCInitStructure);	//PC7，预留
	TIM_OC4Init(TIM8, &TIM_OCInitStructure);	//PC9，Motor C PWM
	//TIM8是高级定时器，必须打开主输出开关
	TIM_CtrlPWMOutputs(TIM8, ENABLE);
	TIM_Cmd(TIM8, ENABLE);
	
	GPIO_SetBits(GPIOC,GPIO_Pin_2);
	GPIO_ResetBits(GPIOB,GPIO_Pin_14);
}

void MotorA_Init(void){
	
}
void MotorB_Init(void){
/* 接口配置
 * MotorA PB5 -- AIN1	PB4 -- AIN2		pwm->C7-CCR2
 * MotorB PD2 -- BIN1	PC12 -- BIN2	pwm->C6-CCR1
 * MotorC PB8 -- CIN1	PB9 -- CIN2	    pwm->C9-CCR4
 * MotorD PC14 -- DIN1	PC13 -- DIN2	pwm->C8-CCR3
 */
	//开启GPIOC D 时钟
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOD, ENABLE);
	//PD2 PC12 GPIO_Init
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;		//PD2
	GPIO_Init(GPIOD, &GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;		//PC12
	GPIO_Init(GPIOC, &GPIO_InitStructure);
}
void MotorC_Init(void){
/* 接口配置
 * MotorA PB5 -- AIN1	PB4 -- AIN2		pwm->C7-CCR2
 * MotorB PD2 -- BIN1	PC12 -- BIN2	pwm->C6-CCR1
 * MotorC PB8 -- CIN1	PB9 -- CIN2	    pwm->C9-CCR4
 * MotorD PC14 -- DIN1	PC13 -- DIN2	pwm->C8-CCR3
 */
	//开启 GPIOB 时钟
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
	//PB8 PB9 GPIO_Init
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8 |GPIO_Pin_9;		//PC14 13
	GPIO_Init(GPIOB, &GPIO_InitStructure);
}
void MotorD_Init(void){
/* 接口配置
 * MotorA PB5 -- AIN1	PB4 -- AIN2		pwm->C7-CCR2
 * MotorB PD2 -- BIN1	PC12 -- BIN2	pwm->C6-CCR1
 * MotorC PB8 -- CIN1	PB9 -- CIN2	    pwm->C9-CCR4
 * MotorD PC14 -- DIN1	PC13 -- DIN2	pwm->C8-CCR3
 */
	//开启 GPIOC 时钟
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
	//PC14 PC13 GPIO_Init
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_14 |GPIO_Pin_13;		//PC14 13
	GPIO_Init(GPIOD, &GPIO_InitStructure);
}









