#pragma once

#include <stddef.h>

typedef size_t jmp_buf[32];

__attribute__((returns_twice)) int setjmp(jmp_buf env);
__attribute__((noreturn)) void longjmp(jmp_buf env, int val);