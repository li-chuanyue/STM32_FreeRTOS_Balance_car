#ifndef __MOTOR_DRIVER_H__
#define __MOTOR_DRIVER_H__

#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_tim.h"

typedef enum{
	MOTOR_LEFT,
	MOTOR_RIGHT
}Motor_type;

//直流电机驱动--方向-速度
/* speed范围：-50.0～50.0，内部映射到高分辨率PWM。 */
void Motor_control(Motor_type motor, float speed);
//Motor init function
void Motor_init(void);
void MotorA_Init(void);
void MotorB_Init(void);
void MotorC_Init(void);
void MotorD_Init(void);

#endif
