#pragma once

#define USE_BUILTIN_MEMCPY
#define USE_BUILTIN_MEMSET

#ifdef USE_BUILTIN_MEMCPY
#define memcpy(dst, src, n) __builtin_memcpy(dst, src, n)
#else
void* memcpy(void* restrict dst, const void* restrict src, size_t n);
#endif

#ifdef USE_BUILTIN_MEMCPY
#define memset(dst, c, n) __builtin_memset(dst, c, n)
#else
void* memset(void* addr, int c, size_t n);
#endif
