#ifndef __OLED_H
#define __OLED_H

#include <stdint.h>

#define OLED_WIDTH   128U
#define OLED_HEIGHT   64U

void OLED_Init(void);
void OLED_Clear(void);
void OLED_Fill(uint8_t color);
void OLED_Update(void);
void OLED_SetContrast(uint8_t contrast);

void OLED_DrawPixel(uint8_t x, uint8_t y, uint8_t color);
void OLED_ShowChar(uint8_t x, uint8_t y, char character);
void OLED_ShowString(uint8_t x, uint8_t y, const char *string);
void OLED_ShowInt(uint8_t x, uint8_t y, int32_t number);

#endif
