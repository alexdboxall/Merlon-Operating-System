#pragma once

/*
 * IMPLEMENTS STANDARD 
 */

#define static_assert _Static_assert

#ifdef COMPILE_KERNEL

#ifndef NDEBUG

#define ___tostr(x) #x
#define __tostr(x) ___tostr(x)

_Noreturn void AssertionFail(const char* file, const char* line, const char* condition, const char* msg);

#define assert(condition) (condition ? (void)0 : (void)AssertionFail(__FILE__, __tostr(__LINE__), __tostr(condition), ""));
#define assert_with_message(condition, msg) (condition ? (void)0 : (void)AssertionFail(__FILE__, __tostr(__LINE__), __tostr(condition), msg));

#else

#define assert(condition)
#define assert_with_message(condition, msg)

#endif

#else

#ifdef NDEBUG
#define assert(ignore) ((void)0)
#else

#define ___tostr(x) #x
#define __tostr(x) ___tostr(x)

#include <stdio.h>
extern FILE* stderr;

extern int fprintf(FILE* restrict stream, const char* restrict format, ...);

#define assert(condition) {if (!(condition)) {fprintf(stderr, "Assertion failed: %s, function %s, file %s, line %s.", __tostr(condition), __tostr(__func__), __tostr(__FILE__), __tostr(__LINE__));}}
#endif

#endif