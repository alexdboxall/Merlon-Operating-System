
#include <panic.h>
#include <arch.h>
#include <log.h>
#include <debug.h>
#include <irql.h>

static char* message_table[_PANIC_HIGHEST_VALUE] = {
	[PANIC_UNKNOWN] 							= "unknown",
	[PANIC_IMPOSSIBLE_RETURN] 					= "impossible return",
	[PANIC_MANUALLY_INITIATED]					= "manually initiated",
	[PANIC_UNIT_TEST_OK]						= "unit test ok",
	[PANIC_DRIVER_FAULT]						= "driver fault",
	[PANIC_OUT_OF_HEAP]							= "out of heap",
	[PANIC_OUT_OF_BOOTSTRAP_HEAP]				= "out of boostrap heap",
	[PANIC_HEAP_REQUEST_TOO_LARGE]				= "heap request too large",
	[PANIC_ASSERTION_FAILURE] 					= "assertion failure",
	[PANIC_NO_MEMORY_MAP]						= "no memory map",
	[PANIC_NOT_IMPLEMENTED] 					= "not implemented",
	[PANIC_INVALID_IRQL]						= "invalid irq level",
	[PANIC_SPINLOCK_WRONG_IRQL]					= "spinlock wrong irql",
	[PANIC_PRIORITY_QUEUE]						= "invalid operation on a priority queue",
	[PANIC_OUT_OF_PHYS]							= "no physical memory left",
	[PANIC_LINKED_LIST]							= "invalid operation on a linked list",
	[PANIC_CANARY_DIED]							= "kernel stack overflow detected",
	[PANIC_SEMAPHORE_DESTROY_WHILE_HELD]		= "tried to destroy a held semaphore",
	[PANIC_SEM_BLOCK_WITHOUT_THREAD] 			= "semaphore acquisition would block before multithreading has started",
	[PANIC_CANNOT_LOCK_MEMORY]					= "cannot lock virtual memory",
	[PANIC_THREAD_LIST]							= "invalid operation on a thread list",
	[PANIC_CANNOT_MALLOC_WITHOUT_FAULTING]		= "heap allocation cannot be completed without faulting",
	[PANIC_NO_FILESYSTEM]						= "no driver can access the boot filesystem",
	[PANIC_BAD_KERNEL]							= "kernel executable file is corrupted",
	[PANIC_DISK_FAILURE_ON_SWAPFILE]			= "failed to read from or write to swapfile",
	[PANIC_NEGATIVE_SEMAPHORE]					= "more semaphore releases than acquisitions",
	[PANIC_NON_MASKABLE_INTERRUPT]				= "unrecoverable hardware error",
	[PANIC_UNHANDLED_KERNEL_EXCEPTION]			= "unhandled cpu exception",
	[PANIC_REQUIRED_DRIVER_MISSING_SYMBOL]		= "driver is missing a required symbol",
	[PANIC_REQUIRED_DRIVER_NOT_FOUND]			= "a required driver could not be loaded",
	[PANIC_NO_LOW_MEMORY]						= "not enough conventional memory to satisfy request",
	[PANIC_OUT_OF_SWAPFILE]						= "out of swapfile space",
};

[[noreturn]] void Panic(int code)
{
	char* msg = code < _PANIC_HIGHEST_VALUE ? message_table[code] : "";
	PanicEx(code, msg);
}

[[noreturn]] void PanicEx(int code, const char* message) {
	LogWriteSerial("PANIC %d %s\n", code, message);
	if (IsInTfwTest()) {
		LogWriteSerial("in test.\n");
		FinishedTfwTest(code);
		ArchSetPowerState(ARCH_POWER_STATE_REBOOT);
	}

	RaiseIrql(IRQL_HIGH);
	LogWriteSerial("\n\n *** KERNEL PANIC ***\n\n0x%X - %s\n", code, message);

	while (1) {
		ArchDisableInterrupts();
		ArchStallProcessor();
	}
}