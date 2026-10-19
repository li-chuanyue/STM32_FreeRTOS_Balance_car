#include "oled.h"
#include "oled_font.h"
#include "stm32f10x.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"

/*==========================================================================
 * USER CONFIGURATION AREA
 *
 * Change only this area to match the four signal wires on your board.
 * The module has no exposed CS pin, so the driver does not control CS.
 *==========================================================================*/

#define OLED_GPIO_CLOCKS  (RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC)

#define OLED_SCL_PORT     GPIOB
#define OLED_SCL_PIN      GPIO_Pin_13

#define OLED_SDA_PORT     GPIOB
#define OLED_SDA_PIN      GPIO_Pin_12

#define OLED_RES_PORT     GPIOC
#define OLED_RES_PIN      GPIO_Pin_4

#define OLED_DC_PORT      GPIOA
#define OLED_DC_PIN       GPIO_Pin_5

/* SH1106 normally uses 2; SSD1306 normally uses 0. */
#define OLED_COLUMN_OFFSET  0U

/*==========================================================================*/

#define OLED_SCL_HIGH() GPIO_SetBits(OLED_SCL_PORT, OLED_SCL_PIN)
#define OLED_SCL_LOW()  GPIO_ResetBits(OLED_SCL_PORT, OLED_SCL_PIN)
#define OLED_SDA_HIGH() GPIO_SetBits(OLED_SDA_PORT, OLED_SDA_PIN)
#define OLED_SDA_LOW()  GPIO_ResetBits(OLED_SDA_PORT, OLED_SDA_PIN)
#define OLED_RES_HIGH() GPIO_SetBits(OLED_RES_PORT, OLED_RES_PIN)
#define OLED_RES_LOW()  GPIO_ResetBits(OLED_RES_PORT, OLED_RES_PIN)
#define OLED_DC_HIGH()  GPIO_SetBits(OLED_DC_PORT, OLED_DC_PIN)
#define OLED_DC_LOW()   GPIO_ResetBits(OLED_DC_PORT, OLED_DC_PIN)

static uint8_t OLED_Buffer[OLED_WIDTH * OLED_HEIGHT / 8U];

static void OLED_DelayCycles(volatile uint32_t count)
{
    while (count-- != 0U)
    {
        __NOP();
    }
}

static void OLED_GPIO_Init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(OLED_GPIO_CLOCKS, ENABLE);

    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;

    gpio.GPIO_Pin = OLED_SCL_PIN;
    GPIO_Init(OLED_SCL_PORT, &gpio);
    gpio.GPIO_Pin = OLED_SDA_PIN;
    GPIO_Init(OLED_SDA_PORT, &gpio);
    gpio.GPIO_Pin = OLED_RES_PIN;
    GPIO_Init(OLED_RES_PORT, &gpio);
    gpio.GPIO_Pin = OLED_DC_PIN;
    GPIO_Init(OLED_DC_PORT, &gpio);

    OLED_SCL_LOW();
    OLED_SDA_LOW();
    OLED_DC_LOW();
    OLED_RES_HIGH();
}

static void OLED_SPI_WriteByte(uint8_t value)
{
    uint8_t bit;

    for (bit = 0U; bit < 8U; ++bit)
    {
        OLED_SCL_LOW();

        if ((value & 0x80U) != 0U)
            OLED_SDA_HIGH();
        else
            OLED_SDA_LOW();

        OLED_SCL_HIGH();
        value <<= 1;
    }

    OLED_SCL_LOW();
}

static void OLED_WriteCommand(uint8_t command)
{
    OLED_DC_LOW();
    OLED_SPI_WriteByte(command);
}

static void OLED_WriteData(uint8_t data)
{
    OLED_DC_HIGH();
    OLED_SPI_WriteByte(data);
}

static const uint8_t *OLED_GetGlyph(char character)
{
    static const uint8_t glyph_minus[5] = {0x08, 0x08, 0x08, 0x08, 0x08};
    static const uint8_t glyph_plus[5]  = {0x08, 0x08, 0x3E, 0x08, 0x08};
    static const uint8_t glyph_dot[5]   = {0x00, 0x60, 0x60, 0x00, 0x00};
    static const uint8_t glyph_colon[5] = {0x00, 0x36, 0x36, 0x00, 0x00};
    static const uint8_t glyph_slash[5] = {0x20, 0x10, 0x08, 0x04, 0x02};
    static const uint8_t glyph_equal[5] = {0x14, 0x14, 0x14, 0x14, 0x14};
    static const uint8_t glyph_under[5] = {0x40, 0x40, 0x40, 0x40, 0x40};
    static const uint8_t glyph_query[5] = {0x02, 0x01, 0x51, 0x09, 0x06};

    if ((character >= '0') && (character <= '9'))
        return OLED_Font5x7[1U + (uint8_t)(character - '0')];

    if ((character >= 'a') && (character <= 'z'))
        character = (char)(character - 'a' + 'A');

    if ((character >= 'A') && (character <= 'Z'))
        return OLED_Font5x7[11U + (uint8_t)(character - 'A')];

    switch (character)
    {
        case ' ': return OLED_Font5x7[0];
        case '-': return glyph_minus;
        case '+': return glyph_plus;
        case '.': return glyph_dot;
        case ':': return glyph_colon;
        case '/': return glyph_slash;
        case '=': return glyph_equal;
        case '_': return glyph_under;
        default:  return glyph_query;
    }
}

void OLED_Init(void)
{
    OLED_GPIO_Init();

    OLED_RES_LOW();
    OLED_DelayCycles(720000U);
    OLED_RES_HIGH();
    OLED_DelayCycles(720000U);

    OLED_WriteCommand(0xAE); /* display off */
    OLED_WriteCommand(0xD5); OLED_WriteCommand(0x80); /* clock */
    OLED_WriteCommand(0xA8); OLED_WriteCommand(0x3F); /* multiplex 1/64 */
    OLED_WriteCommand(0xD3); OLED_WriteCommand(0x00); /* display offset */
    OLED_WriteCommand(0x40); /* start line 0 */
    OLED_WriteCommand(0xA1); /* segment remap */
    OLED_WriteCommand(0xC8); /* COM scan direction */
    OLED_WriteCommand(0xDA); OLED_WriteCommand(0x12); /* COM pins */
    OLED_WriteCommand(0x81); OLED_WriteCommand(0x7F); /* contrast */
    OLED_WriteCommand(0xD9); OLED_WriteCommand(0xF1); /* pre-charge */
    OLED_WriteCommand(0xDB); OLED_WriteCommand(0x40); /* VCOM detect */
    OLED_WriteCommand(0x8D); OLED_WriteCommand(0x14); /* charge pump */
    OLED_WriteCommand(0xA4); /* display follows RAM */
    OLED_WriteCommand(0xA6); /* normal display */

    OLED_Clear();
    OLED_Update();
    OLED_WriteCommand(0xAF); /* display on */
}

void OLED_Clear(void)
{
    OLED_Fill(0U);
}

void OLED_Fill(uint8_t color)
{
    uint16_t index;
    uint8_t value = (color != 0U) ? 0xFFU : 0x00U;

    for (index = 0U; index < sizeof(OLED_Buffer); ++index)
        OLED_Buffer[index] = value;
}

void OLED_Update(void)
{
    uint8_t page;
    uint8_t column;

    for (page = 0U; page < 8U; ++page)
    {
        OLED_WriteCommand((uint8_t)(0xB0U + page));
        OLED_WriteCommand((uint8_t)(0x00U | (OLED_COLUMN_OFFSET & 0x0FU)));
        OLED_WriteCommand((uint8_t)(0x10U | ((OLED_COLUMN_OFFSET >> 4U) & 0x0FU)));

        for (column = 0U; column < OLED_WIDTH; ++column)
            OLED_WriteData(OLED_Buffer[(uint16_t)page * OLED_WIDTH + column]);
    }
}

void OLED_SetContrast(uint8_t contrast)
{
    OLED_WriteCommand(0x81);
    OLED_WriteCommand(contrast);
}

void OLED_DrawPixel(uint8_t x, uint8_t y, uint8_t color)
{
    uint16_t index;
    uint8_t mask;

    if ((x >= OLED_WIDTH) || (y >= OLED_HEIGHT))
        return;

    index = (uint16_t)(y / 8U) * OLED_WIDTH + x;
    mask = (uint8_t)(1U << (y & 7U));

    if (color != 0U)
        OLED_Buffer[index] |= mask;
    else
        OLED_Buffer[index] &= (uint8_t)(~mask);
}

void OLED_ShowChar(uint8_t x, uint8_t y, char character)
{
    const uint8_t *glyph = OLED_GetGlyph(character);
    uint8_t column;
    uint8_t row;

    for (column = 0U; column < 5U; ++column)
    {
        for (row = 0U; row < 7U; ++row)
            OLED_DrawPixel((uint8_t)(x + column), (uint8_t)(y + row),
                           (uint8_t)((glyph[column] >> row) & 1U));
    }

    for (row = 0U; row < 7U; ++row)
        OLED_DrawPixel((uint8_t)(x + 5U), (uint8_t)(y + row), 0U);
}

void OLED_ShowString(uint8_t x, uint8_t y, const char *string)
{
    if (string == 0)
        return;

    while (*string != '\0')
    {
        if ((uint16_t)x + 5U >= OLED_WIDTH)
        {
            x = 0U;
            y = (uint8_t)(y + 8U);
        }

        if ((uint16_t)y + 7U >= OLED_HEIGHT)
            break;

        OLED_ShowChar(x, y, *string++);
        x = (uint8_t)(x + 6U);
    }
}

void OLED_ShowInt(uint8_t x, uint8_t y, int32_t number)
{
    char text[12];
    uint8_t length = 0U;
    uint8_t index;
    uint32_t magnitude;

    if (number < 0)
    {
        text[length++] = '-';
        magnitude = (uint32_t)(-(number + 1)) + 1U;
    }
    else
    {
        magnitude = (uint32_t)number;
    }

    index = length;
    do
    {
        text[length++] = (char)('0' + (magnitude % 10U));
        magnitude /= 10U;
    } while (magnitude != 0U);

    {
        uint8_t left = index;
        uint8_t right = (uint8_t)(length - 1U);
        while (left < right)
        {
            char temporary = text[left];
            text[left] = text[right];
            text[right] = temporary;
            ++left;
            --right;
        }
    }

    text[length] = '\0';
    OLED_ShowString(x, y, text);
}
