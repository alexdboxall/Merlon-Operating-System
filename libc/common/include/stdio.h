#pragma once

#ifndef COMPILE_KERNEL

#include <stddef.h>
#include <stdarg.h>

#ifndef NULL
#define NULL	((void*) 0)
#endif

#define FOPEN_MAX 4096
#define EOF (-1)

#define _IOFBF 0
#define _IOLBF 1
#define _IONBF 2
#define BUFSIZ 512

struct FILE;
struct DIR;
typedef struct DIR DIR;
typedef struct FILE FILE;

extern FILE* stdin;
extern FILE* stdout;
extern FILE* stderr;

FILE* fopen(const char* filename, const char* mode);
FILE* freopen(const char* filename, const char* mode, FILE* stream);
FILE* fmemopen(void* buffer, size_t size, const char* mode);
int fclose(FILE* stream);

int feof(FILE* stream);
int ferror(FILE* stream);
void clearerr(FILE* stream);

void flockfile(FILE* stream);
int ftrylockfile(FILE* stream);
void funlockfile(FILE* stream);

/*
* The 'core' read/write functions that (for now) everything else will be
* implemented in terms of. Obviously we will implement optimised fread/fwrite
* later on to increase performance.
*/
int fputc(int c, FILE *stream);
int fgetc(FILE* stream);


char* fgets(char* buffer, int n, FILE* stream);
int getc(FILE* stream);
int getchar(void);
int ungetc(int c, FILE* stream);

int fputs(const char* s, FILE* stream);
int puts(const char* s);
int putc(int c, FILE* stream);
int putchar(int c);

int fflush(FILE* stream);

int fileno(FILE* stream);

int setvbuf(FILE* stream, char* buf, int mode, size_t size);
void setbuf(FILE* stream, char* buf);
void setbuffer(FILE* stream, char* buf, size_t size);
void setlinebuf(FILE* stream);

int vfprintf(FILE* stream, const char* format, va_list ap);
int vsnprintf(char* str, size_t size, const char* format, va_list ap);
int vsprintf(char* str, const char* format, va_list ap);
int vprintf(const char* format, va_list ap);
int fprintf(FILE* stream, const char* format, ...);
int printf(const char* format, ...);
int sprintf(char* str, const char* format, ...);
int snprintf(char* str, size_t size, const char* format, ...);

int remove(const char* path);

struct dirent;
DIR* opendir(const char* name);
struct dirent* readdir(DIR *dirp);
int closedir(DIR* dirp);

void perror(const char* s);

#endif