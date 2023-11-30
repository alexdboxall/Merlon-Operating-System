#pragma once

#include <common.h>

export void LogWriteSerial(const char* format, ...);
export void LogDeveloperWarning(const char* format, ...);

void DbgScreenPrintf(const char* format, ...);
void DbgScreenPuts(char* str);
void DbgScreenPutchar(char c);