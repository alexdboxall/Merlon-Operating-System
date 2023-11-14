#include <assert.h>
#include <panic.h>
#include <arch.h>
#include <log.h>
#include <debug.h>

_Noreturn void AssertionFail(const char* file, const char* line, const char* condition, const char* msg) {
	ArchDisableInterrupts();
	LogWriteSerial("Assertion failed: %s %s [%s: %s]\n", condition, msg, file, line);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wframe-address"
	if (!IsInTfwTest()) {
		LogWriteSerial("Stack trace:\n");
		LogWriteSerial("    0x%X\n", (size_t) __builtin_return_address(0));
		LogWriteSerial("    0x%X\n", (size_t) __builtin_return_address(1));
		LogWriteSerial("    0x%X\n", (size_t) __builtin_return_address(2));
	}
#pragma GCC diagnostic pop

	Panic(PANIC_ASSERTION_FAILURE);
}