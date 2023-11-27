#pragma once

#include <stddef.h>

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
int strcoll(const char* s1, const char* s2);
int strncmp(const char* s1, const char* s2, size_t n);
size_t strxfrm(char* restrict dst, const char* restrict src, size_t n);

void* memchr(const void* s, int c, size_t n);
char* strchr(const char* s, int c);
size_t strcspn(const char* s1, const char* s2);
char* strpbrk(const char* s1, const char* s2);

void bzero(void* addr, size_t n);
void* memccpy(void* dst, const void* src, int c, size_t n);
void* memset(void* addr, int c, size_t n);
char* strcpy(char* dst, const char* src);
char* strdup(const char* str);
char* strerror(int err);
size_t strlen(const char* str);
char* strrchr(const char* str, int n);
char* strstr(const char* haystac, const char* needle);
