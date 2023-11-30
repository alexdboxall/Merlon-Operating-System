#pragma once

/*
 * IMPLEMENTS STANDARD - NEEDS TESTING THOUGH (ESP. STRTOK, STRSTR, etc.)
 */

#include <stddef.h>

#ifdef COMPILE_KERNEL
#include <common.h>
#define EXPORT_
#else
#define EXPORT_
#endif

#ifndef NULL
#define NULL ((void*) 0)
#endif

EXPORT_ void* memcpy(void* restrict dst, const void* restrict src, size_t n);
EXPORT_ void* memmove(void* dst, const void* src, size_t n);
EXPORT_ char* strcpy(char* restrict dst, const char* restrict src);
EXPORT_ char* strncpy(char* restrict dst, const char* restrict src, size_t n);
EXPORT_ char* strcat(char* restrict dst, const char* restrict src);
EXPORT_ char* strncat(char* restrict dst, const char* restrict src, size_t n);
EXPORT_ int memcmp(const void* s1, const void* s2, size_t n);
EXPORT_ int strcmp(const char* s1, const char* s2);
EXPORT_ void* memchr(const void* s, int c, size_t n);

EXPORT_ void* memset(void* addr, int c, size_t n);
EXPORT_ size_t strlen(const char* str);
EXPORT_ void bzero(void* addr, size_t n);
EXPORT_ char* strdup(const char* str);

#ifndef COMPILE_KERNEL
int strcoll(const char* s1, const char* s2);
int strncmp(const char* s1, const char* s2, size_t n);
size_t strxfrm(char* restrict dst, const char* restrict src, size_t n);
char* strchr(const char* s, int c);
size_t strcspn(const char* s1, const char* s2);
char* strpbrk(const char* s1, const char* s2);
char* strrchr(const char* s, int c);
size_t strspn(const char* s1, const char* s2);
char* strstr(const char* haystack, const char* needle);
char* strtok(char* restrict s, const char* restrict delim);
char* strerror(int err);
#endif