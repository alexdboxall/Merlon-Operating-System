#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>

#define export __attribute__((used))

#ifndef NULL
#define NULL ((void*) 0)
#endif

#define warn_unused __attribute__((warn_unused_result))
#define always_inline __attribute__((always_inline)) inline

#define inline_memcpy(dst, src, n) __builtin_memcpy(dst, src, n)
#define inline_memset(dst, v, n) __builtin_memset(dst, v, n)