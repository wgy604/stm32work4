#ifndef OLED_H
#define OLED_H

#include <stdint.h>

void OLED_Init(void);
void OLED_Task(void);
void OLED_Clear(void);
void OLED_ShowChar(uint8_t line, uint8_t column, char ch);
void OLED_ShowString(uint8_t line, uint8_t column, const char *text);

#endif
