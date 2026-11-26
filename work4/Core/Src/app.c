#include "app.h"
#include "app_config.h"
#include "main.h"
#include "tim.h"
#include "usart.h"
#include "servo.h"
#include "OLED.h"

#include <stdio.h>

typedef struct
{
    uint16_t pin;
    GPIO_PinState raw;
    GPIO_PinState stable;
    uint32_t changed_at;
} Key;

static const int sequence[] = {0, 45, 90, 135, 180};
static Key keys[2];
static int target_angle;
static uint8_t sequence_index;
static uint8_t automatic;
static uint32_t last_change;

static void ApplyTarget(int angle)
{
    if (angle < 0)
        angle = 0;
    if (angle > 180)
        angle = 180;

    target_angle = angle;
    uint16_t ccr = Servo_SetAngle(target_angle);
    char line[17];

    snprintf(line, sizeof(line), "Target:%03udeg",
             (unsigned int)target_angle);
    OLED_ShowString(1U, 1U, line);
    snprintf(line, sizeof(line), "CCR   :%4u", (unsigned int)ccr);
    OLED_ShowString(2U, 1U, line);
    snprintf(line, sizeof(line), "Pulse :%u.%03ums",
             (unsigned int)(ccr / 1000U), (unsigned int)(ccr % 1000U));
    OLED_ShowString(3U, 1U, line);

    char message[80];
    int length = snprintf(message, sizeof(message),
        "Target=%03u  CCR=%4u  Pulse=%u.%03ums\r\n",
        (unsigned int)target_angle, (unsigned int)ccr,
        (unsigned int)(ccr / 1000U), (unsigned int)(ccr % 1000U));
    if (length <= 0 || length >= (int)sizeof(message))
    {
        Error_Handler();
        return;
    }
    if (HAL_UART_Transmit(&huart1, (uint8_t *)message,
                          (uint16_t)length, 100U) != HAL_OK)
        Error_Handler();
}

static void Key_Task(uint32_t now)
{
    for (unsigned int i = 0U; i < 2U; i++)
    {
        Key *key = &keys[i];
        GPIO_PinState raw = HAL_GPIO_ReadPin(GPIOB, key->pin);
        if (raw != key->raw)
        {
            key->raw = raw;
            key->changed_at = now;
        }
        if (raw != key->stable &&
            (uint32_t)(now - key->changed_at) >= KEY_DEBOUNCE_MS)
        {
            key->stable = raw;
            if (raw == KEY_PRESSED)
            {
                automatic = 0U;
                ApplyTarget(target_angle + (i == 0U ? 15 : -15));
            }
        }
    }
}

void App_Init(void)
{
    OLED_Init();
    keys[0].pin = KEY_PLUS_Pin;
    keys[1].pin = KEY_MINUS_Pin;
    for (unsigned int i = 0U; i < 2U; i++)
    {
        keys[i].raw = HAL_GPIO_ReadPin(GPIOB, keys[i].pin);
        keys[i].stable = keys[i].raw;
        keys[i].changed_at = HAL_GetTick();
    }

    if (HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1) != HAL_OK)
        Error_Handler();

    sequence_index = 0U;
    automatic = 1U;
    ApplyTarget(sequence[sequence_index]);
    last_change = HAL_GetTick();
}

void App_Task(void)
{
    uint32_t now = HAL_GetTick();
    Key_Task(now);
    if (automatic != 0U &&
        (uint32_t)(now - last_change) >= SEQUENCE_HOLD_MS)
    {
        last_change = now;
        if (sequence_index + 1U < sizeof(sequence) / sizeof(sequence[0]))
        {
            sequence_index++;
            ApplyTarget(sequence[sequence_index]);
        }
        else
            automatic = 0U;
    }
    OLED_Task();
}
