
#include <common.h>
#include <log.h>

__attribute__((no_instrument_function)) static void IntToStr(uint32_t i, char* output, int base)
{
	const char* digits = "0123456789ABCDEF";

    /*
    * Work out where the end of the string is (this is based on the number).
    * Using the do...while ensures that we always get at least one digit 
    * (i.e. ensures a 0 is printed if the input was 0).
    */
	uint32_t shifter = i;
	do {
		++output;
		shifter /= base;
	} while (shifter);

    /* Put in the null terminator. */
	*output = '\0';

    /*
    * Now fill in the digits back-to-front.
    */
	do {
		*--output = digits[i % base];
		i /= base;
	} while (i);
}

__attribute__((no_instrument_function)) static void outb(uint16_t port, uint8_t value)
{
	asm volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

__attribute__((no_instrument_function)) static uint8_t inb(uint16_t port)
{
	uint8_t value;
	asm volatile ("inb %1, %0"
		: "=a"(value)
		: "Nd"(port));
	return value;
}

__attribute__((no_instrument_function)) static void logcnv(char c, bool screen)
{	
	while ((inb(0x3F8 + 5) & 0x20) == 0) {
		;
	}
	outb(0x3F8, c);
	if (screen) {
		DbgScreenPutchar(c);
	}
}

__attribute__((no_instrument_function)) static void logsnv(char* a, bool screen)
{
	while (*a) logcnv(*a++, screen);
}

__attribute__((no_instrument_function)) static void log_intnv(uint32_t i, int base, bool screen)
{
	char str[12];
    IntToStr(i, str, base);
	logsnv(str, screen);
}

__attribute__((no_instrument_function)) static void LogWriteSerialVa(const char* format, va_list list, bool screen) {
	if (format == NULL) {
		format = "NULL";
	}

	int i = 0;

	while (format[i]) {
		if (format[i] == '%') {
			switch (format[++i]) {
			case '%': 
				logcnv('%', screen); break;
			case 'c':
				logcnv(va_arg(list, int), screen); break;
			case 's': 
				logsnv(va_arg(list, char*), screen); break;
			case 'd': 
				log_intnv(va_arg(list, signed), 10, screen); break;
			case 'x':
			case 'X': 
				log_intnv(va_arg(list, unsigned), 16, screen); break;
			case 'l':
			case 'L': 
				log_intnv(va_arg(list, unsigned long long), 16, screen); break;
			case 'u':
				log_intnv(va_arg(list, unsigned), 10, screen); break;
			default: 
				logcnv('%', screen); 
				logcnv(format[i], screen);
				break;
			}
		} else {
			logcnv(format[i], screen);
		}
		i++;
	}
}

__attribute__((no_instrument_function)) void LogWriteSerial(const char* format, ...)
{
	va_list list;
	va_start(list, format);
	LogWriteSerialVa(format, list, false);
	va_end(list);
}

__attribute__((no_instrument_function)) void LogDeveloperWarning(const char* format, ...) {
	va_list list;
	va_start(list, format);
	LogWriteSerial("\n!!!!!!!!!!!!!!!!!!!!\n\n>>> KERNEL DEVELOPER WARNING:\n    ");
	LogWriteSerialVa(format, list, false);
	va_end(list);
}

__attribute__((no_instrument_function)) void DbgScreenPrintf(const char* format, ...) {
	va_list list;
	va_start(list, format);
	LogWriteSerialVa(format, list, true);
	va_end(list);
}

static size_t call_stack[1024];
static size_t call_stack_ptr = 0;

#include <irql.h>

__attribute__((no_instrument_function)) void __cyg_profile_func_enter(void *this_fn, void *call_site) {
	(void) this_fn;
	(void) call_site;
	call_stack[call_stack_ptr++] = (size_t) this_fn;

	if (GetIrql() >= IRQL_DRIVER) {
		return;
	}

	/*LogWriteSerial("CALL STACK: ");
	for (size_t i = 0; i < call_stack_ptr; ++i) {
		LogWriteSerial("0x%X, ", call_stack[i]);
	}
	LogWriteSerial("\n");*/
}

__attribute__((no_instrument_function)) void __cyg_profile_func_exit(void *this_fn, void *call_site) {
	(void) this_fn;
	(void) call_site;
	--call_stack_ptr;
	//LogWriteSerial("EXIT: 0x%X -> 0x%X\n", this_fn, call_site);
}