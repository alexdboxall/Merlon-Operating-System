#include <syscall.h>
#include <log.h>
#include <errno.h>
#include <_syscallnum.h>

typedef int (*system_call_t)(size_t, size_t, size_t, size_t, size_t);

static const char* syscall_names[_SYSCALL_NUM_ENTRIES] = {
	[SYSCALL_YIELD] = "yield",
	[SYSCALL_TERMINATE] = "terminate",
	[SYSCALL_MAPVIRT] = "map_virt",
	[SYSCALL_UNMAPVIRT] = "unmap_virt",
	[SYSCALL_OPEN] = "open",
	[SYSCALL_READ] = "read",
	[SYSCALL_WRITE] = "write",
	[SYSCALL_CLOSE] = "close",
	[SYSCALL_SEEK] = "seek",
};

static const system_call_t system_call_table[_SYSCALL_NUM_ENTRIES] = {
	[SYSCALL_YIELD] = SysYield,
	[SYSCALL_TERMINATE] = SysTerminate,
	[SYSCALL_MAPVIRT] = SysMapVirt,
	[SYSCALL_UNMAPVIRT] = SysUnmapVirt,
	[SYSCALL_OPEN] = SysOpen,
	[SYSCALL_READ] = NULL,
	[SYSCALL_WRITE] = NULL,
	[SYSCALL_CLOSE] = NULL,
	[SYSCALL_SEEK] = NULL,
};

int HandleSystemCall(int call, size_t a, size_t b, size_t c, size_t d, size_t e) {
	if (call >= _SYSCALL_NUM_ENTRIES) {
		return ENOSYS;
	}

	LogWriteSerial("Calling system call: %s, with args 0x%X 0x%X 0x%X 0x%X 0x%X\n", syscall_names[call], a, b, c, d, e);

	if (system_call_table[call] == NULL) {
		return ENOSYS;
	}

	return system_call_table[call](a, b, c, d, e);
}