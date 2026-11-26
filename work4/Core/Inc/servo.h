#ifndef SERVO_H
#define SERVO_H

#include <stdint.h>

uint16_t Servo_AngleToCCR(int angle);
uint16_t Servo_SetAngle(int angle);

#endif
