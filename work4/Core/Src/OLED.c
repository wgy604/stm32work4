#include "OLED.h"
#include "OLED_Font.h"
#include "app_config.h"
#include "main.h"

#include <string.h>

static char desired[4][16];
static char shown[4][16];
static uint8_t address;
static uint8_t online;
static uint8_t scan_position;
static uint32_t last_attempt;
static uint32_t cycles_per_us;

static void DelayUs(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    while ((uint32_t)(DWT->CYCCNT - start) < us * cycles_per_us)
    {
    }
}

static void Sda(GPIO_PinState level)
{
    HAL_GPIO_WritePin(OLED_SDA_GPIO_Port, OLED_SDA_Pin, level);
}

static void SclLow(void)
{
    HAL_GPIO_WritePin(OLED_SCL_GPIO_Port, OLED_SCL_Pin, GPIO_PIN_RESET);
    DelayUs(2U);
}

static uint8_t SclHigh(void)
{
    uint32_t start = DWT->CYCCNT;
    HAL_GPIO_WritePin(OLED_SCL_GPIO_Port, OLED_SCL_Pin, GPIO_PIN_SET);
    while (HAL_GPIO_ReadPin(OLED_SCL_GPIO_Port, OLED_SCL_Pin) == GPIO_PIN_RESET)
    {
        if ((uint32_t)(DWT->CYCCNT - start) >= 100U * cycles_per_us)
            return 0U;
    }
    DelayUs(2U);
    return 1U;
}

static uint8_t Start(void)
{
    Sda(GPIO_PIN_SET);
    DelayUs(2U);
    if (!SclHigh() || HAL_GPIO_ReadPin(OLED_SDA_GPIO_Port, OLED_SDA_Pin) == GPIO_PIN_RESET)
        return 0U;
    Sda(GPIO_PIN_RESET);
    DelayUs(2U);
    SclLow();
    return 1U;
}

static void Stop(void)
{
    SclLow();
    Sda(GPIO_PIN_RESET);
    DelayUs(2U);
    (void)SclHigh();
    Sda(GPIO_PIN_SET);
    DelayUs(2U);
}

static uint8_t WriteByte(uint8_t byte)
{
    for (unsigned int bit = 0; bit < 8U; bit++)
    {
        Sda((byte & 0x80U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        byte = (uint8_t)(byte << 1);
        DelayUs(2U);
        if (!SclHigh())
            return 0U;
        SclLow();
    }
    Sda(GPIO_PIN_SET);
    DelayUs(2U);
    if (!SclHigh())
        return 0U;
    uint8_t ack = HAL_GPIO_ReadPin(OLED_SDA_GPIO_Port, OLED_SDA_Pin) == GPIO_PIN_RESET;
    SclLow();
    return ack;
}

static uint8_t Probe(uint8_t candidate)
{
    uint8_t ok = Start();
    if (ok)
        ok = WriteByte((uint8_t)(candidate << 1));
    Stop();
    return ok;
}

static uint8_t Send(uint8_t control, const uint8_t *data, uint8_t length)
{
    uint8_t ok = Start();
    if (ok)
        ok = WriteByte((uint8_t)(address << 1));
    if (ok)
        ok = WriteByte(control);
    for (unsigned int i = 0; ok && i < length; i++)
        ok = WriteByte(data[i]);
    Stop();
    return ok;
}

static uint8_t Connect(void)
{
    Sda(GPIO_PIN_SET);
    for (unsigned int i = 0; i < 9U; i++)
    {
        SclLow();
        if (!SclHigh())
        {
            Stop();
            return 0U;
        }
    }
    Stop();

    if (OLED_ADDRESS != 0U)
        address = OLED_ADDRESS;
    else if (Probe(0x3CU))
        address = 0x3CU;
    else
        address = 0x3DU;
    if (!Probe(address))
        return 0U;

    static const uint8_t commands[] = {
        0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40,
        0xA1, 0xC8, 0xDA, 0x12, 0x81, 0xCF, 0xD9, 0xF1,
        0xDB, 0x30, 0xA4, 0xA6,
#if OLED_SH1106
        0xAD, 0x8B,
#else
        0x20, 0x02, 0x8D, 0x14,
#endif
        0xAF
    };
    if (!Send(0x00U, commands, sizeof(commands)))
        return 0U;
    memset(shown, 0xFF, sizeof(shown));
    return 1U;
}

static uint8_t WriteChar(uint8_t row, uint8_t column, char ch)
{
    uint8_t x = (uint8_t)(column * 8U + (OLED_SH1106 ? 2U : 0U));
    const uint8_t *glyph = OLED_F8x16[(unsigned int)ch - ' '];
    for (unsigned int half = 0; half < 2U; half++)
    {
        uint8_t commands[] = {
            (uint8_t)(0xB0U | (row * 2U + half)),
            (uint8_t)(0x10U | (x >> 4)),
            (uint8_t)(x & 0x0FU)
        };
        if (!Send(0x00U, commands, sizeof(commands)) ||
            !Send(0x40U, glyph + half * 8U, 8U))
            return 0U;
    }
    return 1U;
}

void OLED_Init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    cycles_per_us = SystemCoreClock / 1000000U;
    if (cycles_per_us == 0U)
        cycles_per_us = 1U;
    memset(desired, ' ', sizeof(desired));
    online = 0U;
    scan_position = 0U;
    last_attempt = HAL_GetTick() - 1000U;
}

void OLED_Clear(void)
{
    memset(desired, ' ', sizeof(desired));
}

void OLED_ShowChar(uint8_t line, uint8_t column, char ch)
{
    if (line < 1U || line > 4U || column < 1U || column > 16U)
        return;
    if (ch < ' ' || ch > '~')
        ch = '?';
    desired[line - 1U][column - 1U] = ch;
}

void OLED_ShowString(uint8_t line, uint8_t column, const char *text)
{
    while (*text != '\0' && column <= 16U)
        OLED_ShowChar(line, column++, *text++);
}

void OLED_Task(void)
{
    if (online == 0U)
    {
        uint32_t now = HAL_GetTick();
        if ((uint32_t)(now - last_attempt) >= 1000U)
        {
            last_attempt = now;
            online = Connect();
        }
        return;
    }
    for (unsigned int count = 0; count < 64U; count++)
    {
        uint8_t row = scan_position / 16U;
        uint8_t column = scan_position % 16U;
        scan_position = (uint8_t)((scan_position + 1U) % 64U);
        if (desired[row][column] != shown[row][column])
        {
            char ch = desired[row][column];
            if (WriteChar(row, column, ch))
                shown[row][column] = ch;
            else
            {
                online = 0U;
                last_attempt = HAL_GetTick();
            }
            return;
        }
    }
}
