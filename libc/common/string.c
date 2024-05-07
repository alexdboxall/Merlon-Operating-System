
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>

/*
 * Contains the code for memcpy, memset, strcpy, strcmp and strlen.
 */
#include <cmn_string.h>

#ifdef COMPILE_KERNEL
#include <heap.h>
#include <common.h>
#else
#include <stdlib.h>
#define export
#endif

export void* memchr(const void* s, int c, size_t n) {
	const uint8_t* ptr = (const uint8_t*) s;

	while (n--) {
		if (*ptr == (uint8_t) c) {
			return (void*) ptr;
		}

		++ptr;
	}

	return NULL;
}

int memcmp(const void* s1, const void* s2, size_t n) {
    const uint8_t* a = (const uint8_t*) s1;
	const uint8_t* b = (const uint8_t*) s2;

	for (size_t i = 0; i < n; ++i) {
		if (a[i] < b[i]) return -1;
		else if (a[i] > b[i]) return 1;
	}
	
	return 0;
}

int strncmp(const char* s1, const char* s2, size_t n) {
	while (n && *s1 && (*s1 == *s2)) {
		++s1;
		++s2;
		--n;
	}
	if (n == 0) {
		return 0;
	} else {
		return (*(unsigned char*) s1 - *(unsigned char*) s2);
	}
}

export void* memmove(void* dst, const void* src, size_t n) {
	uint8_t* a = (uint8_t*) dst;
	const uint8_t* b = (const uint8_t*) src;

	if (a <= b) {
		while (n--) {
			*a++ = *b++;
		}
	} else {
		b += n;
		a += n;

		while (n--) {
			*--a = *--b;
		}
	}

	return dst;
}

export char* strcat(char* restrict dst, const char* restrict src) {
	char* ret = dst;

	while (*dst) {
		++dst;
	}

	while ((*dst++ = *src++)) {
		;
	}

	return ret;
}

export char* strncpy(char* restrict dst, const char* restrict src, size_t n) {
	char* ret = dst;

	while (n--) {
		if (*src) {
			*dst++ = *src++;
		} else {
			*dst++ = 0;
		}
	}

	return ret;
}

export char* strchr(const char* s, int c) {
	do {
		if (*s == (char) c) {
			return (char*) s;
		}
	} while (*s++);
	return NULL;
}

export char* strdup(const char* str) {
	char* copy = (char*) malloc(strlen(str) + 1);
	strcpy(copy, str);
	return copy;
}

#ifndef COMPILE_KERNEL

int strcasecmp(const char* s1, const char* s2) {
	while ((*s1) && (tolower(*s1) == tolower(*s2))) {
		++s1;
		++s2;
	}

	return tolower(*(uint8_t*) s1) - tolower(*(uint8_t*) s2);
}

int strncasecmp(const char* s1, const char* s2, size_t n) {
	while (n && *s1 && (tolower(*s1) == tolower(*s2))) {
		++s1;
		++s2;
		--n;
	}
	if (n == 0) {
		return 0;
	} else {
		return tolower(*(unsigned char*) s1) - tolower(*(unsigned char*) s2);
	}
}

char* strerror(int err) {
	switch (err) {
	case 0:
		return "Success";
	case ENOSYS:
		return "Not implemented";
	case ENOMEM:
		return "Not enough memory";
	case ENODEV:
		return "No device";
	case EALREADY:
		return "Driver has already been asssigned";
	case ENOTSUP:
		return "Operation not supported";
	case EDOM:
		return "Argument outside of domain of function";
	case EINVAL:
		return "Invalid argument";
	case EEXIST:
		return "File already exists";
	case ENOENT:
		return "No such file or directory";
	case EIO:
		return "Input / output error";
	case EACCES:
		return "Permission denied";
	case ENOSPC:
		return "No space left on device";
	case ENAMETOOLONG:
		return "Filename too long";
	case ENOTDIR:
		return "Not a directory";
	case EISDIR:
		return "Is a directory";
	case ELOOP:
		return "Too many loops in symbolic link resolution";
	case EROFS:
		return "Read-only filesystem";
	case EAGAIN: /* == EWOULDBLOCK */
		return "Resource temporarily unavailable / Operation would block";
    case EFAULT:
        return "Bad address";
    case EBADF:
        return "Bad file descriptor";
    case ENOTTY:
        return "Not a terminal";
    case ERANGE:
        return "Result of out range";
    case EILSEQ:
        return "Illegal byte sequence";
    case EMFILE:
        return "Too many open files";
    case ENFILE:
        return "Too many open files in system";
    case EPIPE:
        return "Broken pipe";
    case ESPIPE:
        return "Invalid seek";
	case ETIMEDOUT:
		return "Operation timed out";
	case ENOBUFS:
		return "No buffer space";
	case EBUSY:
		return "Resource busy or locked";
	case ENOEXEC:
		return "Invalid executable file";
	case ENOTRECOVERABLE:
		return "Operation has failed past point of no return";
	case EINTR:
		return "Interrupted by signal";
	case ECANCELED:
		return "Operation cancelled";
	default:
		return "Unknown error";
	}
}

char* strncat(char* restrict dst, const char* restrict src, size_t n) {
	char* ret = dst;

	while (*dst) {
		++dst;
	}

	while (*src && n--) {
		*dst++ = *src++;
	}	

	*dst = 0;
	return ret;
}

int strcoll(const char* s1, const char* s2) {
	int size1 = 1 + strxfrm(NULL, s1, 0);
	int size2 = 1 + strxfrm(NULL, s2, 0);

	char out1[size1];
	char out2[size2];

	strxfrm(out1, s1, size1);
	strxfrm(out2, s2, size2);

	return strcmp(out1, out2);
}

size_t strxfrm(char* restrict dst, const char* restrict src, size_t n) {
	if (n == 0) {
		if (dst != NULL) {
			*dst = 0;
		}
		return 0;
	}
	while (n-- > 1) {
		if (*src) {
			*dst++ = *src++;
		} else {
			*dst = 0;
			break;
		}
	}
	*dst = 0;
	return strlen(dst);
}

size_t strspn(const char* s1, const char* s2) {
	size_t i = 0;
	for (; s1[i]; ++i) {
		bool found = false;
		for (size_t j = 0; s2[j]; ++j) {
			if (s1[i] == s2[j]) {
				found = true;
				break;
			}
		}
		if (!found) {
			return i;
		}
	}
	return i;
}

size_t strcspn(const char* s1, const char* s2) {
	size_t i = 0;
	for (; s1[i]; ++i) {
		for (size_t j = 0; s2[j]; ++j) {
			if (s1[i] == s2[j]) {
				return i;
			}
		}
	}
	return i;
}

char* strpbrk(const char* s1, const char* s2) {
	char* s = (char*) s1;
	while (*s) {
		for (size_t i = 0; s2[i]; ++i) {
			if (*s == s2[i]) {
				return s;
			}
		}
		s++;
	}
	return NULL;
}

char* strrchr(const char* s, int c) {
	char* result = NULL;
	while (*s) {
		if (*s == (char) c) {
			result = (char*) s;
		}
		s++;
	}
	return result;
}

char* strstr(const char* haystack, const char* needle) {
	size_t n = strlen(needle);

	if (n == 0) {
		return (char*) haystack;
	}

    while (*haystack) {
		if (!memcmp(haystack, needle, n)) {
            return (char*) haystack;
		}

		haystack++;
	}
        
    return NULL;
}

char* strtok(char* restrict s, const char* restrict delim) {
	static char* saved_pointer = NULL;
	char* token = NULL;

	if (s != NULL) {
		/*
		 * Start a new search. 
		 */
		saved_pointer = s;

	} else if (saved_pointer == NULL) {
		/*
		 * Previous search reached end of string.
		 */
		return NULL;
	}

	/*
	 * Find the next non-delimiter character. If none are found, then we are at 
	 * the end of the string, and so saved_pointer goes to NULL, and will 
	 * continue to be NULL. 
	 */
	for (size_t i = 0; saved_pointer[i]; ++i) {
		bool found = false;
		for (size_t j = 0; delim[j]; ++j) {
			if (delim[j] == saved_pointer[i]) {
				found = true;
				token = saved_pointer + i;
				break;
			}
		}

		if (!found) {
			/*
			 * End of string, no more tokens. 
			 */
			saved_pointer = NULL;
			return NULL;
		}
	}

	for (size_t i = 0; saved_pointer[i]; ++i) {
		for (size_t j = 0; delim[j]; ++j) {
			if (saved_pointer[i] == delim[j]) {
				saved_pointer[i] = 0;
				saved_pointer += i + 1;
				return token;
			}
		}
	}

	/*
	 * Found the final token.
	 */
	saved_pointer = NULL;
	return token;
}

#endif
