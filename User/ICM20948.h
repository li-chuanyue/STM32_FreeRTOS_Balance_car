#ifndef __ICM20948_H
#define __ICM20948_H

#include "stm32f10x.h"

typedef struct
{
    float roll;
    float pitch;
    float yaw;
    float ax, ay, az;
    float gx, gy, gz;
} ICM20948_Data;

uint8_t ICM20948_Init(void);
uint8_t ICM20948_Update(float dt);
const ICM20948_Data *ICM20948_GetData(void);
uint8_t ICM20948_GetWhoAmI(void);
uint8_t ICM20948_GetAddress(void);
void ICM20948_DelayMs(uint32_t ms);

#endif
