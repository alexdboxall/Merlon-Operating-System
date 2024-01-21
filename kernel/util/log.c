
#include <common.h>
#include <log.h>
#include <spinlock.h>
#include <irql.h>

static struct spinlock lock;

#define REAL_HW 0

static void IntToStr(uint32_t i, char* output, int base) {
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

#if REAL_HW == 0
static void outb(uint16_t port, uint8_t value) {
	asm volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static uint8_t inb(uint16_t port) {
	uint8_t value;
	asm volatile ("inb %1, %0"
		: "=a"(value)
		: "Nd"(port));
	return value;
}
#endif

static void LogChar(char c, bool screen) {
	if (screen) {
		DbgScreenPutchar(c);
	}
#if REAL_HW == 0
	while ((inb(0x3F8 + 5) & 0x20) == 0) {
		;
	}
	outb(0x3F8, c);
#endif
}

static void LogStr(char* a, bool screen) {
	while (*a) LogChar(*a++, screen);
}

static void LogInt(uint32_t i, int base, bool screen)
{
	char str[12];
    IntToStr(i, str, base);
	LogStr(str, screen);
}

static void LogWriteSerialVa(const char* format, va_list list, bool screen) {
	if (format == NULL) {
		format = "NULL";
	}

	static bool first_run = true;
	if (first_run) {
		/*
		 * Even IRQL_HIGH is allowed to write to the log, so we have to raise it
		 * up to this level.
		 */
		InitSpinlock(&lock, "log", IRQL_HIGH);
		first_run = false;
	}

	if (!screen) {
		AcquireSpinlock(&lock);
	}

	int i = 0;

	while (format[i]) {
		if (format[i] == '%') {
			switch (format[++i]) {
			case '%': 
				LogChar('%', screen); break;
			case 'c':
				LogChar(va_arg(list, int), screen); break;
			case 's': 
				LogStr(va_arg(list, char*), screen); break;
			case 'd': 
				LogInt(va_arg(list, signed), 10, screen); break;
			case 'x':
			case 'X': 
				LogInt(va_arg(list, unsigned), 16, screen); break;
			case 'l':
			case 'L': 
				LogInt(va_arg(list, unsigned long long), 16, screen); break;
			case 'u':
				LogInt(va_arg(list, unsigned), 10, screen); break;
			default: 
				LogChar('%', screen); 
				LogChar(format[i], screen);
				break;
			}
		} else {
			LogChar(format[i], screen);
		}
		i++;
	}

	if (!screen) {
		ReleaseSpinlock(&lock);
	}
}

void LogWriteSerial(const char* format, ...)
{
	va_list list;
	va_start(list, format);
	LogWriteSerialVa(format, list, false);
	va_end(list);
}

void LogDeveloperWarning(const char* format, ...) {
	va_list list;
	va_start(list, format);
	LogWriteSerial("\n!!!!!!!!!!!!!!!!\n\n>>> KERNEL DEVELOPER WARNING:\n    ");
	LogWriteSerialVa(format, list, false);
	va_end(list);
}

void DbgScreenPrintf(const char* format, ...) {
	va_list list;
	va_start(list, format);
	LogWriteSerialVa(format, list, true);
	va_end(list);
}