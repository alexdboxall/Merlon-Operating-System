#pragma once

#include <common.h>

void LogWriteSerial(const char* format, ...);
void LogDeveloperWarning(const char* format, ...);

#define Log(format, ...) LogWriteSerial("[%s]: " format "\n", __func__, ## __VA_ARGS__)

void DbgScreenPrintf(const char* format, ...);
void DbgScreenPutchar(char c);
