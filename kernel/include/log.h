#pragma once

void LogWriteSerial(const char* format, ...);
void LogDeveloperWarning(const char* format, ...);

void DbgScreenPrintf(const char* format, ...);
void DbgScreenPuts(char* str);
void DbgScreenPutchar(char c);