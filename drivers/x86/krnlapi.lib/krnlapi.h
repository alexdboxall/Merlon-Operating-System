#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <_syscallnum.h>
#include <sys/types.h>

int _system_call(int call, size_t a, size_t b, size_t c, size_t d, size_t e);

void* xmemcpy(void* restrict dst, const void* restrict src, size_t n);
int xstrcmp(const char* s1, const char* s2);
char* xstrcpy(char* restrict dst, const char* restrict src);
size_t xstrlen(const char* str);

void dbgprintf(const char* format, ...);