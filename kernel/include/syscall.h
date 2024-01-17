#pragma once

#include <common.h>

int HandleSystemCall(int call, size_t a, size_t b, size_t c, size_t d, size_t e);

int SysYield(size_t, size_t, size_t, size_t, size_t);
int SysTerminate(size_t, size_t, size_t, size_t, size_t);
int SysMapVirt(size_t, size_t, size_t, size_t, size_t);
int SysUnmapVirt(size_t, size_t, size_t, size_t, size_t);
int SysOpen(size_t, size_t, size_t, size_t, size_t);
int SysReadWrite(size_t, size_t, size_t, size_t, size_t);
int SysClose(size_t, size_t, size_t, size_t, size_t);
int SysSeek(size_t, size_t, size_t, size_t, size_t);
int SysDup(size_t, size_t, size_t, size_t, size_t);
int SysExit(size_t, size_t, size_t, size_t, size_t);
int SysRemove(size_t, size_t, size_t, size_t, size_t);
int SysMprotect(size_t, size_t, size_t, size_t, size_t);
int SysPrepExec(size_t, size_t, size_t, size_t, size_t);
int SysWaitpid(size_t, size_t, size_t, size_t, size_t);
int SysFork(size_t, size_t, size_t, size_t, size_t);
int SysGetPid(size_t, size_t, size_t, size_t, size_t);
int SysGetTid(size_t, size_t, size_t, size_t, size_t);
