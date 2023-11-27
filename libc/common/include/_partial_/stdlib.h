#pragma once

#include <stddef.h>

#ifndef NULL
#define NULL	((void*) 0)
#endif

#define RAND_MAX 0x7FFFFFFF
#define EXIT_FAILURE 1
#define EXIT_SUCCESS 0

#define MB_CUR_MAX ((size_t) 1)



typedef struct {
    int quot;
    int rem;
} div_t;

typedef struct {
    long int quot;
    long int rem;
} ldiv_t;

typedef struct {
    long long int quot;
    long long int rem;
} lldiv_t;

int rand(void);
void srand(unsigned int seed);
void* bsearch(const void* key, const void* base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));
void qsort(void* base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));

int abs(int j);
long int labs(long int j);
long long int llabs(long long int j);

div_t div(int numer, int denom);
ldiv_t ldiv(long int numer, long int denom);
lldiv_t lldiv(long long int numer, long long int denom);