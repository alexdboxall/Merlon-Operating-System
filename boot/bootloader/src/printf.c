#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <util.h>
#include <bootloader.h>

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
    char a[2];
    a[0] = c;
    a[1] = 0;
    Puts(a, BOOTCOL_GREY_ON_BLACK);
}

static void logsnv(char* a)
{
	Puts(a, BOOTCOL_GREY_ON_BLACK);
}

static void log_intnv(uint32_t i, int base)
{
	char str[12];
    IntToStr(i, str, base);
	logsnv(str);
}

static void Vprintf(const char* format, va_list list) {
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

void Printf(const char* format, ...)
{
	va_list list;
	va_start(list, format);
	Vprintf(format, list);
	va_end(list);
}