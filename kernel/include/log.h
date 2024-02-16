#pragma once

#include <common.h>

void LogWriteSerial(const char* format, ...);
void LogWriteSerialVa(const char* format, va_list list, bool screen);
void LogDeveloperWarning(const char* format, ...);

#define Log(format, ...) LogWriteSerial("[%s]: " format "\n", __func__, ## __VA_ARGS__)

void DbgScreenPrintf(const char* format, ...);
void DbgScreenPutchar(char c);
