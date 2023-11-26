#pragma once

#ifndef NULL
#define NULL	((void*) 0)
#endif

#define RAND_MAX 0x7FFFFFFF
#define EXIT_FAILURE 1
#define EXIT_SUCCESS 0

int rand(void);
void srand(unsigned int seed);
