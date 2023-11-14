
#include <panic.h>
#include <arch.h>
#include <log.h>
#include <debug.h>

static char* message_table[_PANIC_HIGHEST_VALUE] = {
	[PANIC_UNKNOWN] 				= "unknown",
	[PANIC_IMPOSSIBLE_RETURN] 		= "impossible return",
	[PANIC_MANUALLY_INITIATED]		= "manually initiated",
	[PANIC_UNIT_TEST_OK]			= "unit test ok",
	[PANIC_DRIVER_FAULT]			= "driver fault",
	[PANIC_OUT_OF_HEAP]				= "out of heap",
	[PANIC_OUT_OF_BOOTSTRAP_HEAP]	= "out of boostrap heap",
	[PANIC_HEAP_REQUEST_TOO_LARGE]	= "heap request too large",
	[PANIC_ASSERTION_FAILURE] 		= "assertion failure",
	[PANIC_NO_MEMORY_MAP]			= "no memory map",
	[PANIC_NOT_IMPLEMENTED] 		= "not implemented"
};

_Noreturn void Panic(int code)
{
	char* msg = code < _PANIC_HIGHEST_VALUE ? message_table[code] : "";
	PanicEx(code, msg);
}

_Noreturn void PanicEx(int code, const char* message) {
	LogWriteSerial("PANIC %d\n", code);
	if (IsInTfwTest()) {
		LogWriteSerial("in test.\n");
		FinishedTfwTest(code);
		ArchReboot();
	}

	LogWriteSerial("\n\n *** KERNEL PANIC ***\n\n0x%X - %s\n", code, message);

	while (1) {
		ArchDisableInterrupts();
		ArchStallProcessor();
	}
}