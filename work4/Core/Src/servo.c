#include "servo.h"
#include "app_config.h"
#include "tim.h"

uint16_t Servo_AngleToCCR(int angle)
{
    if (angle < 0)
        angle = 0;
    if (angle > 180)
        angle = 180;

    uint32_t pulse = 500U + (uint32_t)angle * 2000U / 180U;
    if (pulse < SERVO_MIN_US)
        pulse = SERVO_MIN_US;
    if (pulse > SERVO_MAX_US)
        pulse = SERVO_MAX_US;

    return (uint16_t)pulse;
}

uint16_t Servo_SetAngle(int angle)
{
    uint16_t ccr = Servo_AngleToCCR(angle);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, ccr);
    return ccr;
}
