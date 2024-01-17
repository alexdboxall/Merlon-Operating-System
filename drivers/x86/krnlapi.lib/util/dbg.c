#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <fcntl.h>
#include <unistd.h>
#include "krnlapi.h"

static void IntToStr(uint32_t i, char* output, int base)
{
	const char* digits = "0123456789ABCDEF";
	uint32_t shifter = i;
	do {
		++output;
		shifter /= base;
	} while (shifter);

	*output = '\0';

	do {
		*--output = digits[i % base];
		i /= base;
	} while (i);
}

static void logcnv(char c)
{	
    int fd = open("con:", O_WRONLY, 0);
    write(fd, &c, 1);
    close(fd);
    (void) c;
}

static void logsnv(char* a)
{
	int fd = open("con:", O_WRONLY, 0);
    write(fd, a, xstrlen(a));
    close(fd);
    (void) a;
}

static void log_intnv(uint32_t i, int base)
{
	char str[12];
    IntToStr(i, str, base);
	logsnv(str);
}

static void dbgvprintf(const char* format, va_list list) {
	if (format == NULL) {
		format = "NULL";
	}

	int i = 0;

	while (format[i]) {
		if (format[i] == '%') {
			switch (format[++i]) {
			case '%': 
				logcnv('%'); break;
			case 'c':
				logcnv(va_arg(list, int)); break;
			case 's': 
				logsnv(va_arg(list, char*)); break;
			case 'd': 
				log_intnv(va_arg(list, signed), 10); break;
			case 'x':
			case 'X': 
				log_intnv(va_arg(list, unsigned), 16); break;
			default: 
				logcnv('%'); 
				logcnv(format[i]);
				break;
			}
		} else {
			logcnv(format[i]);
		}
		i++;
	}
}

void dbgprintf(const char* format, ...)
{
	va_list list;
	va_start(list, format);
	dbgvprintf(format, list);
	va_end(list);
}