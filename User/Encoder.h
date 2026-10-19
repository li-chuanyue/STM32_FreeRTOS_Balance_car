#ifndef __ENCODER_H
#define __ENCODER_H

#include "stm32f10x.h"

/*
 * 左轮（Motor C）：PA6/PA7 -> TIM3
 * 右轮（Motor B）：PB6/PB7 -> TIM4
 */
void Encoder_Init(void);

/* 读取本周期内的增量计数，并将对应定时器计数器清零。 */
int16_t Encoder_ReadLeft(void);
int16_t Encoder_ReadRight(void);

#endif
