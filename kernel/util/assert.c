#include <assert.h>
#include <panic.h>
#include <arch.h>
#include <log.h>
#include <debug.h>
#include <common.h>

export _Noreturn void AssertionFail(
	const char* file, const char* line, const char* condition, const char* msg
) {
	ArchDisableInterrupts();
	LogWriteSerial("Assertion failed: %s %s [%s: %s]\n", condition, msg, file, line);
	Panic(PANIC_ASSERTION_FAILURE);
}