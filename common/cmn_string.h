#include <cmn_string_header.h>

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/*
 * memcpy and memset call the GCC's 'builtin' version of the function.
 * These may be optimised for the target platform, which increases efficiency
 * without resorting to writing custom versions for each architecture.
 * If they are not supported by your compiler or platform, or simply end up
 * calling themselves, use please comment out these definitions.
 */

#undef memset
void* memset(void* addr, int c, size_t n)
{
#ifdef USE_BUILTIN_MEMSET
    return __builtin_memset(addr, c, n);
#else
	uint8_t* ptr = (uint8_t*) addr;
	for (size_t i = 0; i < n; ++i) {
		ptr[i] = c;
	}

	return addr;
#endif
}

#undef memcpy
void* memcpy(void* restrict dst, const void* restrict src, size_t n)
{
#ifdef USE_BUILTIN_MEMCPY
    return __builtin_memcpy(dst, src, n);
#else
	uint8_t* a = (uint8_t*) dst;
	const uint8_t* b = (const uint8_t*) src;

	for (size_t i = 0; i < n; ++i) {
		a[i] = b[i];
	}

	return dst;
#endif
}

int strcmp(const char* s1, const char* s2)
{
	while ((*s1) && (*s1 == *s2)) {
		++s1;
		++s2;
	}

	return (*(uint8_t*) s1 - *(uint8_t*) s2);
}

char* strcpy(char* restrict dst, const char* restrict src)
{
	char* ret = dst;

	while ((*dst++ = *src++)) {
		;
	}

	return ret;
}

size_t strlen(const char* str)
{
	size_t len = 0;
	while (str[len]) {
		++len;
	}
	return len;
}
