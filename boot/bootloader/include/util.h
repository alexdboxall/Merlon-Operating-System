#pragma once

#include <bootloader.h>
#include <common.h>

struct firmware_info* GetFw(void);

void Putchar(int x, int y, char c, uint8_t col);
void Clear(void);
void Reboot(void);
int WaitKey(void);
int CheckKey(void);
void ExitBootServices(void);
bool DoesFileExist(const char* filename);
size_t GetFileSize(const char* filename);
void LoadFile(const char* filename, size_t addr);
void Fail(const char* why1, const char* why2);
void Sleep(int milli);
void SetCursor(int x, int y);
void Puts(const char* str, uint8_t col);
void Printf(const char* format, ...);
void DiagnosticPrintf(const char* format, ...);
