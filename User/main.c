#include "stm32f10x.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_i2c.h"
#include "stm32f10x_usart.h"
#include <stdio.h>
#include <math.h>

/* ICM-20948 的 7 位 I2C 地址：此前 WHO_AM_I 已读到 0xEA。 */
#define ICM20948_I2C_ADDR       0x68U
#define ICM20948_REG_BANK_SEL   0x7FU
#define ICM20948_REG_WHO_AM_I   0x00U
#define ICM20948_REG_PWR_MGMT_1 0x06U
#define ICM20948_REG_PWR_MGMT_2 0x07U
#define ICM20948_REG_ACCEL_XH   0x2DU
#define ICM20948_REG_GYRO_XH    0x33U

#define I2C_WAIT_TIMEOUT        100000UL

static void USART1_Init(void);
static uint8_t IIC2_Init(void);
static uint8_t I2C2_WriteReg(uint8_t reg, uint8_t value);
static uint8_t I2C2_ReadReg(uint8_t reg, uint8_t *value);
static uint8_t I2C2_Read2Bytes(uint8_t reg, uint8_t data[2]);
static uint8_t ICM20948_InitForTest(uint8_t *who_am_i);
static int16_t BytesToInt16(const uint8_t data[2]);
static void DelayCycles(volatile uint32_t count);

/* USART1：PA9=TX，PA10=RX；串口助手设为 115200、8N1。 */
static void USART1_Init(void)
{
    GPIO_InitTypeDef gpio;
    USART_InitTypeDef usart;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_USART1,
                           ENABLE);

    gpio.GPIO_Pin = GPIO_Pin_9;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &gpio);

    gpio.GPIO_Pin = GPIO_Pin_10;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &gpio);

    USART_StructInit(&usart);
    usart.USART_BaudRate = 115200U;
    usart.USART_WordLength = USART_WordLength_8b;
    usart.USART_StopBits = USART_StopBits_1;
    usart.USART_Parity = USART_Parity_No;
    usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    usart.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(USART1, &usart);
    USART_Cmd(USART1, ENABLE);
}

/* Keil printf 重定向到 USART1。 */
int fputc(int ch, FILE *stream)
{
    (void)stream;
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET)
    {
    }
    USART_SendData(USART1, (uint8_t)ch);
    return ch;
}

/*
 * I2C2：PB10=SCL、PB11=SDA；板上已有上拉。
 * 若 MCU 在一次 I2C 事务中复位，传感器可能仍把 SDA 拉低；
 * 先用 GPIO 开漏输出最多补发 9 个时钟，并生成 STOP 恢复总线。
 */
static uint8_t IIC2_Init(void)
{
    GPIO_InitTypeDef gpio;
    I2C_InitTypeDef i2c;
    uint8_t pulse;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C2, ENABLE);
    /* 先复位外设，避免硬件 I2C 继续占住引脚。 */
    I2C_DeInit(I2C2);

    gpio.GPIO_Pin = GPIO_Pin_10 | GPIO_Pin_11;
    gpio.GPIO_Mode = GPIO_Mode_Out_OD;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOB, &gpio);

    /* 开漏输出写 1 表示释放总线，由板上上拉电阻拉高。 */
    GPIO_SetBits(GPIOB, GPIO_Pin_10 | GPIO_Pin_11);
    DelayCycles(500U);

    /* SCL 被外部持续拉低时无法恢复，立即报告。 */
    if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_10) == Bit_RESET)
        return 1U;

    /* SDA 若卡低，最多补 9 个时钟，让从机完成残留字节。 */
    for (pulse = 0U;
         (pulse < 9U) &&
         (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_11) == Bit_RESET);
         ++pulse)
    {
        GPIO_ResetBits(GPIOB, GPIO_Pin_10);
        DelayCycles(500U);
        GPIO_SetBits(GPIOB, GPIO_Pin_10);
        DelayCycles(500U);
        if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_10) == Bit_RESET)
            return 1U;
    }

    /* 在 SCL 为高时先拉低再释放 SDA，合成标准 STOP 条件。 */
    GPIO_ResetBits(GPIOB, GPIO_Pin_11);
    DelayCycles(500U);
    GPIO_SetBits(GPIOB, GPIO_Pin_10);
    DelayCycles(500U);
    GPIO_SetBits(GPIOB, GPIO_Pin_11);
    DelayCycles(500U);

    if ((GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_10) == Bit_RESET) ||
        (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_11) == Bit_RESET))
        return 2U;

    /* 总线释放后，再把 PB10/PB11 交还给 I2C2 复用功能。 */
    gpio.GPIO_Mode = GPIO_Mode_AF_OD;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &gpio);

    I2C_StructInit(&i2c);
    i2c.I2C_ClockSpeed = 100000U;
    i2c.I2C_Mode = I2C_Mode_I2C;
    i2c.I2C_DutyCycle = I2C_DutyCycle_2;
    i2c.I2C_OwnAddress1 = 0U;
    i2c.I2C_Ack = I2C_Ack_Enable;
    i2c.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    I2C_Init(I2C2, &i2c);
    I2C_Cmd(I2C2, ENABLE);
    return 0U;
}

static uint8_t I2C2_HasError(void)
{
    return (I2C_GetFlagStatus(I2C2, I2C_FLAG_AF) == SET) ||
           (I2C_GetFlagStatus(I2C2, I2C_FLAG_BERR) == SET) ||
           (I2C_GetFlagStatus(I2C2, I2C_FLAG_ARLO) == SET) ||
           (I2C_GetFlagStatus(I2C2, I2C_FLAG_OVR) == SET);
}

static void I2C2_ClearErrors(void)
{
    if (I2C_GetFlagStatus(I2C2, I2C_FLAG_AF) == SET)
        I2C_ClearFlag(I2C2, I2C_FLAG_AF);
    if (I2C_GetFlagStatus(I2C2, I2C_FLAG_BERR) == SET)
        I2C_ClearFlag(I2C2, I2C_FLAG_BERR);
    if (I2C_GetFlagStatus(I2C2, I2C_FLAG_ARLO) == SET)
        I2C_ClearFlag(I2C2, I2C_FLAG_ARLO);
    if (I2C_GetFlagStatus(I2C2, I2C_FLAG_OVR) == SET)
        I2C_ClearFlag(I2C2, I2C_FLAG_OVR);
}

static uint8_t I2C2_WaitEvent(uint32_t event)
{
    uint32_t timeout = I2C_WAIT_TIMEOUT;

    while (I2C_CheckEvent(I2C2, event) != SUCCESS)
    {
        if (I2C2_HasError() || (timeout-- == 0U))
            return 1U;
    }
    return 0U;
}

static uint8_t I2C2_WaitFlag(uint32_t flag, FlagStatus expected)
{
    uint32_t timeout = I2C_WAIT_TIMEOUT;

    while (I2C_GetFlagStatus(I2C2, flag) != expected)
    {
        /* BUSY 仅等待总线空闲；其它传输标志同时检查外设错误。 */
        if (((flag != I2C_FLAG_BUSY) && I2C2_HasError()) ||
            (timeout-- == 0U))
            return 1U;
    }
    return 0U;
}

static void I2C2_Abort(void)
{
    I2C_GenerateSTOP(I2C2, ENABLE);
    I2C_AcknowledgeConfig(I2C2, ENABLE);
    I2C_NACKPositionConfig(I2C2, I2C_NACKPosition_Current);
    I2C2_ClearErrors();
}

/* 返回 0 表示成功；非 0 表示失败所在阶段。 */
static uint8_t I2C2_WriteReg(uint8_t reg, uint8_t value)
{
    if (I2C2_WaitFlag(I2C_FLAG_BUSY, RESET) != 0U) return 1U;

    I2C_GenerateSTART(I2C2, ENABLE);
    if (I2C2_WaitEvent(I2C_EVENT_MASTER_MODE_SELECT) != 0U) goto error_start;

    I2C_Send7bitAddress(I2C2, (uint8_t)(ICM20948_I2C_ADDR << 1),
                        I2C_Direction_Transmitter);
    if (I2C2_WaitEvent(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED) != 0U)
        goto error_address;

    I2C_SendData(I2C2, reg);
    if (I2C2_WaitEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED) != 0U)
        goto error_register;

    I2C_SendData(I2C2, value);
    if (I2C2_WaitEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED) != 0U)
        goto error_data;

    I2C_GenerateSTOP(I2C2, ENABLE);
    if (I2C2_WaitFlag(I2C_FLAG_BUSY, RESET) != 0U) return 8U;
    return 0U;

error_data:
    I2C2_Abort();
    return 5U;
error_register:
    I2C2_Abort();
    return 4U;
error_address:
    I2C2_Abort();
    return 3U;
error_start:
    I2C2_Abort();
    return 2U;
}

/* 读取一个寄存器；所有等待均有限时，不会因无应答永久卡死。 */
static uint8_t I2C2_ReadReg(uint8_t reg, uint8_t *value)
{
    if (I2C2_WaitFlag(I2C_FLAG_BUSY, RESET) != 0U) return 1U;

    I2C_AcknowledgeConfig(I2C2, ENABLE);
    I2C_GenerateSTART(I2C2, ENABLE);
    if (I2C2_WaitEvent(I2C_EVENT_MASTER_MODE_SELECT) != 0U) goto error_start;

    I2C_Send7bitAddress(I2C2, (uint8_t)(ICM20948_I2C_ADDR << 1),
                        I2C_Direction_Transmitter);
    if (I2C2_WaitEvent(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED) != 0U)
        goto error_address_write;

    I2C_SendData(I2C2, reg);
    if (I2C2_WaitEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED) != 0U)
        goto error_register;

    I2C_GenerateSTART(I2C2, ENABLE); /* repeated START */
    if (I2C2_WaitEvent(I2C_EVENT_MASTER_MODE_SELECT) != 0U)
        goto error_restart;

    I2C_Send7bitAddress(I2C2, (uint8_t)(ICM20948_I2C_ADDR << 1),
                        I2C_Direction_Receiver);
    if (I2C2_WaitEvent(I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED) != 0U)
        goto error_address_read;

    I2C_AcknowledgeConfig(I2C2, DISABLE); /* 本次只读取一个字节 */
    I2C_GenerateSTOP(I2C2, ENABLE);
    if (I2C2_WaitFlag(I2C_FLAG_RXNE, SET) != 0U) goto error_receive;
    *value = I2C_ReceiveData(I2C2);
    I2C_AcknowledgeConfig(I2C2, ENABLE);
    if (I2C2_WaitFlag(I2C_FLAG_BUSY, RESET) != 0U) return 8U;
    return 0U;

error_receive:
    I2C2_Abort();
    return 7U;
error_address_read:
    I2C2_Abort();
    return 6U;
error_restart:
    I2C2_Abort();
    return 5U;
error_register:
    I2C2_Abort();
    return 4U;
error_address_write:
    I2C2_Abort();
    return 3U;
error_start:
    I2C2_Abort();
    return 2U;
}

/* 连续读两个字节，适用于一个轴的高、低字节。 */
static uint8_t I2C2_Read2Bytes(uint8_t reg, uint8_t data[2])
{
    if (I2C2_WaitFlag(I2C_FLAG_BUSY, RESET) != 0U) return 1U;

    /* STM32F1 的两字节接收专用时序：POS=Next、ACK=1 开始。 */
    I2C_NACKPositionConfig(I2C2, I2C_NACKPosition_Next);
    I2C_AcknowledgeConfig(I2C2, ENABLE);
    I2C_GenerateSTART(I2C2, ENABLE);
    if (I2C2_WaitEvent(I2C_EVENT_MASTER_MODE_SELECT) != 0U) goto error_start;

    I2C_Send7bitAddress(I2C2, (uint8_t)(ICM20948_I2C_ADDR << 1),
                        I2C_Direction_Transmitter);
    if (I2C2_WaitEvent(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED) != 0U)
        goto error_address_write;

    I2C_SendData(I2C2, reg);
    if (I2C2_WaitEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED) != 0U)
        goto error_register;

    I2C_GenerateSTART(I2C2, ENABLE);
    if (I2C2_WaitEvent(I2C_EVENT_MASTER_MODE_SELECT) != 0U)
        goto error_restart;

    I2C_Send7bitAddress(I2C2, (uint8_t)(ICM20948_I2C_ADDR << 1),
                        I2C_Direction_Receiver);
    if (I2C2_WaitEvent(I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED) != 0U)
        goto error_address_read;

    /*
     * EV6 后立即关闭 ACK；BTF=1 时 STOP，然后连续读两次 DR。
     * 这是 RM0008 中 N=2 的接收序列，POS=Next 使末字节收到 NACK。
     */
    I2C_AcknowledgeConfig(I2C2, DISABLE);
    if (I2C2_WaitFlag(I2C_FLAG_BTF, SET) != 0U) goto error_receive;
    I2C_GenerateSTOP(I2C2, ENABLE);
    data[0] = I2C_ReceiveData(I2C2);
    data[1] = I2C_ReceiveData(I2C2);
    I2C_AcknowledgeConfig(I2C2, ENABLE);
    I2C_NACKPositionConfig(I2C2, I2C_NACKPosition_Current);
    if (I2C2_WaitFlag(I2C_FLAG_BUSY, RESET) != 0U) return 8U;
    return 0U;

error_receive:
    I2C2_Abort();
    return 7U;
error_address_read:
    I2C2_Abort();
    return 6U;
error_restart:
    I2C2_Abort();
    return 5U;
error_register:
    I2C2_Abort();
    return 4U;
error_address_write:
    I2C2_Abort();
    return 3U;
error_start:
    I2C2_Abort();
    return 2U;
}

static uint8_t ICM20948_SelectBank(uint8_t bank)
{
    return I2C2_WriteReg(ICM20948_REG_BANK_SEL, (uint8_t)(bank << 4));
}

/* 唤醒芯片并配置 ±2g、±250dps；采样输出约 100Hz。 */
static uint8_t ICM20948_InitForTest(uint8_t *who_am_i)
{
    uint8_t result;

    result = ICM20948_SelectBank(0U);
    if (result != 0U) return 10U + result;
    result = I2C2_ReadReg(ICM20948_REG_WHO_AM_I, who_am_i);
    if (result != 0U) return 20U + result;
    if (*who_am_i != 0xEAU) return 30U;

    result = I2C2_WriteReg(ICM20948_REG_PWR_MGMT_1, 0x01U);
    if (result != 0U) return 40U + result;
    result = I2C2_WriteReg(ICM20948_REG_PWR_MGMT_2, 0x00U);
    if (result != 0U) return 50U + result;
    DelayCycles(1500000UL);

    result = ICM20948_SelectBank(2U);
    if (result != 0U) return 60U + result;
    result = I2C2_WriteReg(0x00U, 10U); /* GYRO_SMPLRT_DIV */
    if (result != 0U) return 70U + result;
    result = I2C2_WriteReg(0x01U, 0x11U); /* ±250dps，启用 DLPF */
    if (result != 0U) return 80U + result;
    result = I2C2_WriteReg(0x10U, 0x00U); /* ACCEL_SMPLRT_DIV_1 */
    if (result != 0U) return 90U + result;
    result = I2C2_WriteReg(0x11U, 10U); /* ACCEL_SMPLRT_DIV_2 */
    if (result != 0U) return 100U + result;
    result = I2C2_WriteReg(0x14U, 0x11U); /* ±2g，启用 DLPF */
    if (result != 0U) return 110U + result;

    result = ICM20948_SelectBank(0U);
    if (result != 0U) return 120U + result;
    return 0U;
}

static int16_t BytesToInt16(const uint8_t data[2])
{
    int32_t value = ((int32_t)data[0] << 8) | data[1];
    if ((value & 0x8000L) != 0L) value -= 65536L;
    return (int16_t)value;
}

static void DelayCycles(volatile uint32_t count)
{
    while (count-- != 0U)
    {
        __NOP();
    }
}

int main(void)
{
    uint8_t who_am_i = 0U;
    uint8_t bytes[2];
    uint8_t error;
    uint8_t bus_recovery;
    int16_t ax, ay, az, gx, gy, gz;
    float ax_g, ay_g, az_g;
    int32_t roll_cdeg, pitch_cdeg;
    int32_t angle_abs;

    USART1_Init();
    bus_recovery = IIC2_Init();
    printf("ICM20948 hardware I2C test start\r\n");
    printf("I2C2 RECOVERY=%u BUSY=%u SCL=%u SDA=%u ERR=%u\r\n",
           (unsigned int)bus_recovery,
           (unsigned int)I2C_GetFlagStatus(I2C2, I2C_FLAG_BUSY),
           (unsigned int)GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_10),
           (unsigned int)GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_11),
           (unsigned int)I2C2_HasError());

    if (bus_recovery != 0U)
    {
        printf("I2C bus recovery failed: code=%u (1=SCL held low, 2=bus lines still low)\r\n",
               (unsigned int)bus_recovery);
        for (;;) {}
    }

    error = ICM20948_InitForTest(&who_am_i);
    if (error != 0U)
    {
        printf("ICM init failed: code=%u, WHO_AM_I=0x%02X\r\n",
               (unsigned int)error, (unsigned int)who_am_i);
        for (;;) {}
    }
    printf("ICM init OK: WHO_AM_I=0x%02X, addr=0x%02X\r\n",
           (unsigned int)who_am_i, ICM20948_I2C_ADDR);

    /* 暂时用主循环验证硬件链路；确认稳定后再移入 FreeRTOS 任务。 */
    for (;;)
    {
        error = I2C2_Read2Bytes(ICM20948_REG_ACCEL_XH, bytes);
        if (error != 0U) goto read_error;
        ax = BytesToInt16(bytes);
        error = I2C2_Read2Bytes((uint8_t)(ICM20948_REG_ACCEL_XH + 2U), bytes);
        if (error != 0U) goto read_error;
        ay = BytesToInt16(bytes);
        error = I2C2_Read2Bytes((uint8_t)(ICM20948_REG_ACCEL_XH + 4U), bytes);
        if (error != 0U) goto read_error;
        az = BytesToInt16(bytes);

        error = I2C2_Read2Bytes(ICM20948_REG_GYRO_XH, bytes);
        if (error != 0U) goto read_error;
        gx = BytesToInt16(bytes);
        error = I2C2_Read2Bytes((uint8_t)(ICM20948_REG_GYRO_XH + 2U), bytes);
        if (error != 0U) goto read_error;
        gy = BytesToInt16(bytes);
        error = I2C2_Read2Bytes((uint8_t)(ICM20948_REG_GYRO_XH + 4U), bytes);
        if (error != 0U) goto read_error;
        gz = BytesToInt16(bytes);

        /* 本测试配置为 ±2g；用加速度估算静态横滚角和俯仰角。 */
        ax_g = (float)ax / 16384.0f;
        ay_g = (float)ay / 16384.0f;
        az_g = (float)az / 16384.0f;
        roll_cdeg = (int32_t)(atan2f(ay_g, az_g) * 5729.57795f);
        pitch_cdeg = (int32_t)(atan2f(-ax_g,
                            sqrtf(ay_g * ay_g + az_g * az_g)) * 5729.57795f);

        printf("ACC raw X=%d Y=%d Z=%d | GYRO raw X=%d Y=%d Z=%d | ",
               (int)ax, (int)ay, (int)az,
               (int)gx, (int)gy, (int)gz);

        angle_abs = (roll_cdeg < 0) ? -roll_cdeg : roll_cdeg;
        printf("Roll=%s%ld.%02ld deg | ", (roll_cdeg < 0) ? "-" : "",
               (long)(angle_abs / 100), (long)(angle_abs % 100));
        angle_abs = (pitch_cdeg < 0) ? -pitch_cdeg : pitch_cdeg;
        printf("Pitch=%s%ld.%02ld deg\r\n", (pitch_cdeg < 0) ? "-" : "",
               (long)(angle_abs / 100), (long)(angle_abs % 100));
        continue;

read_error:
        printf("I2C read failed: code=%u BUSY=%u SCL=%u SDA=%u ERR=%u\r\n",
               (unsigned int)error,
               (unsigned int)I2C_GetFlagStatus(I2C2, I2C_FLAG_BUSY),
               (unsigned int)GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_10),
               (unsigned int)GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_11),
               (unsigned int)I2C2_HasError());
        DelayCycles(3000000UL);
    }
}
