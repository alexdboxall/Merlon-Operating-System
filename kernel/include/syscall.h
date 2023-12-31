#pragma once

#include <common.h>

int HandleSystemCall(int call, size_t a, size_t b, size_t c, size_t d, size_t e);

int SysYield(size_t, size_t, size_t, size_t, size_t);
int SysTerminate(size_t, size_t, size_t, size_t, size_t);
int SysMapVirt(size_t, size_t, size_t, size_t, size_t);
int SysUnmapVirt(size_t, size_t, size_t, size_t, size_t);
int SysOpen(size_t, size_t, size_t, size_t, size_t);
int SysRead(size_t, size_t, size_t, size_t, size_t);
int SysWrite(size_t, size_t, size_t, size_t, size_t);
int SysClose(size_t, size_t, size_t, size_t, size_t);
int SysSeek(size_t, size_t, size_t, size_t, size_t);