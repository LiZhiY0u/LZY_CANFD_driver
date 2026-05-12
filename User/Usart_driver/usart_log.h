#ifndef __USART_LOG_H__
#define __USART_LOG_H__

#include "usart.h"
#include <stdio.h>
#include "stdint.h"
#include <string.h>
#include <stdarg.h>

void Vofa_FireWater(const char *format, ...);
void Vofa_JustFloat(float *_data, uint8_t _num);

#endif