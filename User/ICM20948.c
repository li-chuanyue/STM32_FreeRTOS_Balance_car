#include "ICM20948.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include <math.h>

/* C12H板载连接：PB10=SCL，PB11=SDA。 */
#define IIC_PORT       GPIOB
#define IIC_SCL        GPIO_Pin_10
#define IIC_SDA        GPIO_Pin_11
#define BANK_SEL       0x7FU
#define WHO_AM_I_REG   0x00U
#define ACCEL_XOUT_H   0x2DU

static uint8_t address = 0x69U;
static uint8_t who_am_i;
static float gx_offset, gy_offset, gz_offset;
static ICM20948_Data data;

static void delay_us(uint32_t us)
{
    volatile uint32_t n;
    while (us-- != 0U)
        for (n = 0U; n < 12U; ++n) __NOP();
}

void ICM20948_DelayMs(uint32_t ms)
{
    while (ms-- != 0U) delay_us(1000U);
}

static void SCL_H(void) { GPIO_SetBits(IIC_PORT, IIC_SCL); }
static void SCL_L(void) { GPIO_ResetBits(IIC_PORT, IIC_SCL); }
static void SDA_H(void) { GPIO_SetBits(IIC_PORT, IIC_SDA); }
static void SDA_L(void) { GPIO_ResetBits(IIC_PORT, IIC_SDA); }
static uint8_t SDA_Read(void)
{
    return GPIO_ReadInputDataBit(IIC_PORT, IIC_SDA) == Bit_SET ? 1U : 0U;
}

static void IIC_Init(void)
{
    GPIO_InitTypeDef gpio;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    gpio.GPIO_Pin = IIC_SCL | IIC_SDA;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode = GPIO_Mode_Out_OD;
    GPIO_Init(IIC_PORT, &gpio);
    SCL_H();
    SDA_H();
    ICM20948_DelayMs(2U);
}

static void IIC_Start(void)
{
    SDA_H(); SCL_H(); delay_us(4U);
    SDA_L(); delay_us(4U); SCL_L();
}

static void IIC_Stop(void)
{
    SDA_L(); delay_us(2U); SCL_H(); delay_us(4U); SDA_H(); delay_us(4U);
}

static uint8_t IIC_WriteByte(uint8_t value)
{
    uint8_t i;
    uint16_t timeout;
    for (i = 0U; i < 8U; ++i)
    {
        if ((value & 0x80U) != 0U) SDA_H(); else SDA_L();
        delay_us(2U); SCL_H(); delay_us(4U); SCL_L();
        value <<= 1;
    }
    SDA_H();                         /* 释放SDA，让从机返回ACK */
    delay_us(2U); SCL_H();
    timeout = 1000U;
    while (SDA_Read() != 0U)
    {
        if (timeout == 0U)
        {
            SCL_L();
            return 1U;
        }
        --timeout;
    }
    SCL_L();
    return 0U;
}

static uint8_t IIC_ReadByte(uint8_t ack)
{
    uint8_t i, value = 0U;
    SDA_H();
    for (i = 0U; i < 8U; ++i)
    {
        value <<= 1;
        SCL_H(); delay_us(4U);
        if (SDA_Read() != 0U) value |= 1U;
        SCL_L(); delay_us(2U);
    }
    if (ack != 0U) SDA_L(); else SDA_H();
    SCL_H(); delay_us(4U); SCL_L(); SDA_H();
    return value;
}

static uint8_t WriteReg(uint8_t reg, uint8_t value)
{
    IIC_Start();
    if (IIC_WriteByte((uint8_t)(address << 1)) != 0U) { IIC_Stop(); return 1U; }
    if (IIC_WriteByte(reg) != 0U) { IIC_Stop(); return 2U; }
    if (IIC_WriteByte(value) != 0U) { IIC_Stop(); return 3U; }
    IIC_Stop();
    return 0U;
}

static uint8_t ReadRegs(uint8_t reg, uint8_t *buffer, uint8_t length)
{
    uint8_t i;
    IIC_Start();
    if (IIC_WriteByte((uint8_t)(address << 1)) != 0U) { IIC_Stop(); return 1U; }
    if (IIC_WriteByte(reg) != 0U) { IIC_Stop(); return 2U; }
    IIC_Start();
    if (IIC_WriteByte((uint8_t)((address << 1) | 1U)) != 0U) { IIC_Stop(); return 3U; }
    for (i = 0U; i < length; ++i)
        buffer[i] = IIC_ReadByte((i + 1U < length) ? 1U : 0U);
    IIC_Stop();
    return 0U;
}

static uint8_t SelectBank(uint8_t bank)
{
    return WriteReg(BANK_SEL, (uint8_t)(bank << 4));
}

static int16_t ToInt16(uint8_t high, uint8_t low)
{
    return (int16_t)(((uint16_t)high << 8) | low);
}

static uint8_t ReadMotion(ICM20948_Data *out)
{
    uint8_t raw[12];
    if (SelectBank(0U) != 0U) return 1U;
    if (ReadRegs(ACCEL_XOUT_H, raw, 12U) != 0U) return 2U;
    out->ax = (float)ToInt16(raw[0], raw[1]) / 16384.0f;
    out->ay = (float)ToInt16(raw[2], raw[3]) / 16384.0f;
    out->az = (float)ToInt16(raw[4], raw[5]) / 16384.0f;
    out->gx = (float)ToInt16(raw[6], raw[7]) / 131.0f - gx_offset;
    out->gy = (float)ToInt16(raw[8], raw[9]) / 131.0f - gy_offset;
    out->gz = (float)ToInt16(raw[10], raw[11]) / 131.0f - gz_offset;
    return 0U;
}

uint8_t ICM20948_Init(void)
{
    uint16_t i;
    uint8_t id = 0U;
    ICM20948_Data sample;
    float gx_sum = 0.0f, gy_sum = 0.0f, gz_sum = 0.0f;
    IIC_Init();

    /* 原理图为0x69；同时自动兼容0x68的硬件版本。 */
    address = 0x69U;
    if ((SelectBank(0U) != 0U) || (ReadRegs(WHO_AM_I_REG, &id, 1U) != 0U) || (id != 0xEAU))
    {
        address = 0x68U;
        if ((SelectBank(0U) != 0U) || (ReadRegs(WHO_AM_I_REG, &id, 1U) != 0U) || (id != 0xEAU))
        {
            who_am_i = id;
            return 1U;
        }
    }
    who_am_i = id;

    WriteReg(0x06U, 0x80U);          /* 软件复位 */
    ICM20948_DelayMs(100U);
    SelectBank(0U);
    if (WriteReg(0x06U, 0x01U) != 0U) return 2U; /* 唤醒，自动选时钟 */
    if (WriteReg(0x07U, 0x00U) != 0U) return 3U;

    SelectBank(2U);
    WriteReg(0x00U, 0x03U);          /* 约281Hz，满足250Hz控制环 */
    WriteReg(0x01U, 0x11U);          /* ±250dps，DLPF */
    WriteReg(0x10U, 0x00U);
    WriteReg(0x11U, 0x03U);          /* 约281Hz，满足250Hz控制环 */
    WriteReg(0x14U, 0x11U);          /* ±2g，DLPF */
    SelectBank(0U);
    ICM20948_DelayMs(20U);

    /* 校准期间板子必须静止。 */
    gx_offset = gy_offset = gz_offset = 0.0f;
    for (i = 0U; i < 300U; ++i)
    {
        if (ReadMotion(&sample) != 0U) return 4U;
        /* 校准期间offset保持0，独立累计原始角速度。 */
        gx_sum += sample.gx;
        gy_sum += sample.gy;
        gz_sum += sample.gz;
        ICM20948_DelayMs(2U);
    }
    gx_offset = gx_sum / 300.0f;
    gy_offset = gy_sum / 300.0f;
    gz_offset = gz_sum / 300.0f;

    if (ReadMotion(&data) != 0U) return 5U;
    data.roll = atan2f(data.ay, data.az) * 57.2957795f;
    data.pitch = atan2f(-data.ax, sqrtf(data.ay * data.ay + data.az * data.az)) * 57.2957795f;
    data.yaw = 0.0f;
    return 0U;
}

uint8_t ICM20948_Update(float dt)
{
    float accel_roll, accel_pitch;
    float alpha;
    const float filter_time_constant = 0.50f;

    if ((dt <= 0.0f) || (dt > 0.1f)) return 2U;
    if (ReadMotion(&data) != 0U) return 1U;
    accel_roll = atan2f(data.ay, data.az) * 57.2957795f;
    accel_pitch = atan2f(-data.ax, sqrtf(data.ay * data.ay + data.az * data.az)) * 57.2957795f;
    /*
     * 系数随调用周期自动变化。dt=4ms时alpha约0.992，
     * 避免车辆加速产生的水平加速度被过度当作倾角。
     */
    alpha = filter_time_constant / (filter_time_constant + dt);
    data.roll = alpha * (data.roll + data.gx * dt)
              + (1.0f - alpha) * accel_roll;
    data.pitch = alpha * (data.pitch + data.gy * dt)
               + (1.0f - alpha) * accel_pitch;
    data.yaw += data.gz * dt;
    return 0U;
}

const ICM20948_Data *ICM20948_GetData(void) { return &data; }
uint8_t ICM20948_GetWhoAmI(void) { return who_am_i; }
uint8_t ICM20948_GetAddress(void) { return address; }
