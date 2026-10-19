#include "Encoder.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_tim.h"

static void Encoder_TIM3_Init(void)
{
    GPIO_InitTypeDef gpio;
    TIM_TimeBaseInitTypeDef timer;
    TIM_ICInitTypeDef input;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

    gpio.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio);

    TIM_TimeBaseStructInit(&timer);
    timer.TIM_Prescaler = 0U;
    timer.TIM_Period = 0xFFFFU;
    timer.TIM_CounterMode = TIM_CounterMode_Up;
    timer.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInit(TIM3, &timer);

    TIM_EncoderInterfaceConfig(TIM3, TIM_EncoderMode_TI12,
                               TIM_ICPolarity_Rising,
                               TIM_ICPolarity_Rising);
    TIM_ICStructInit(&input);
    input.TIM_ICFilter = 10U;       /* 过滤电机附近的高频干扰 */
    TIM_ICInit(TIM3, &input);

    TIM_SetCounter(TIM3, 0U);
    TIM_Cmd(TIM3, ENABLE);
}

static void Encoder_TIM4_Init(void)
{
    GPIO_InitTypeDef gpio;
    TIM_TimeBaseInitTypeDef timer;
    TIM_ICInitTypeDef input;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);

    gpio.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &gpio);

    TIM_TimeBaseStructInit(&timer);
    timer.TIM_Prescaler = 0U;
    timer.TIM_Period = 0xFFFFU;
    timer.TIM_CounterMode = TIM_CounterMode_Up;
    timer.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInit(TIM4, &timer);

    TIM_EncoderInterfaceConfig(TIM4, TIM_EncoderMode_TI12,
                               TIM_ICPolarity_Rising,
                               TIM_ICPolarity_Rising);
    TIM_ICStructInit(&input);
    input.TIM_ICFilter = 10U;
    TIM_ICInit(TIM4, &input);

    TIM_SetCounter(TIM4, 0U);
    TIM_Cmd(TIM4, ENABLE);
}

void Encoder_Init(void)
{
    Encoder_TIM3_Init();
    Encoder_TIM4_Init();
}

int16_t Encoder_ReadLeft(void)
{
    int16_t count = (int16_t)TIM_GetCounter(TIM3);
    TIM_SetCounter(TIM3, 0U);

    /* 官方Motor C安装方向与车辆前进方向相反，因此先取负。 */
    return (int16_t)(-count);
}

int16_t Encoder_ReadRight(void)
{
    int16_t count = (int16_t)TIM_GetCounter(TIM4);
    TIM_SetCounter(TIM4, 0U);
    return -count;
}
