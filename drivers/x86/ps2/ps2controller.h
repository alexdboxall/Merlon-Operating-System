#pragma once

#include <common.h>

EXPORT void InitPs2(void);
NO_EXPORT void Ps2ControllerSetIrqEnable(bool enable, bool port2);
void Ps2ControllerDisableDevice(bool port2);
void Ps2ControllerEnableDevice(bool port2);
uint8_t Ps2DeviceRead(void);
int Ps2DeviceWrite(uint8_t data, bool port2);
void Ps2ControllerSetConfiguration(uint8_t value);
uint8_t Ps2ControllerGetConfiguration(void);
int Ps2ControllerTestPort(bool port2);