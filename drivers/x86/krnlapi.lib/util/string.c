
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

void* xmemcpy(void* restrict dst, const void* restrict src, size_t n)
{
    return __builtin_memcpy(dst, src, n);
}

int xstrcmp(const char* s1, const char* s2)
{
	while ((*s1) && (*s1 == *s2)) {
		++s1;
		++s2;
	}

	return (*(uint8_t*) s1 - *(uint8_t*) s2);
}

char* xstrcpy(char* restrict dst, const char* restrict src)
{
	char* ret = dst;

	while ((*dst++ = *src++)) {
		;
	}

	return ret;
}

size_t xstrlen(const char* str)
{
	size_t len = 0;
	while (str[len]) {
		++len;
	}
	return len;
}
