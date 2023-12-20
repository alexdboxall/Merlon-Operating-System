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

void* memcpy(void* restrict dst, const void* restrict src, size_t n);
void* memmove(void* dst, const void* src, size_t n);
char* strcpy(char* restrict dst, const char* restrict src);
char* strncpy(char* restrict dst, const char* restrict src, size_t n);
char* strcat(char* restrict dst, const char* restrict src);
char* strncat(char* restrict dst, const char* restrict src, size_t n);
int memcmp(const void* s1, const void* s2, size_t n);
int strcmp(const char* s1, const char* s2);
void* memchr(const void* s, int c, size_t n);
int strncmp(const char* s1, const char* s2, size_t n);

void* memset(void* addr, int c, size_t n);
size_t strlen(const char* str);
void bzero(void* addr, size_t n);
char* strdup(const char* str);

#ifdef COMPILE_KERNEL
char* strdup_pageable(const char* str);
#endif

#ifndef COMPILE_KERNEL
int strcoll(const char* s1, const char* s2);
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