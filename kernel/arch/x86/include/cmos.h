#pragma once

#include <stdint.h>

uint8_t ReadCmos(uint8_t reg);
void WriteCmos(uint8_t reg, uint8_t data);
void SetNmiEnable(bool enable);
void InitCmos(void);

extern int x86_rtc_century_register;