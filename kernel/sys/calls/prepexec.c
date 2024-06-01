#include <syscall.h>
#include <errno.h>
#include <_syscallnum.h>
#include <thread.h>
#include <process.h>
#include <virtual.h>
#include <filedes.h>
#include <log.h>

int SysPrepExec(size_t, size_t, size_t, size_t, size_t) {
	LogWriteSerial("SysPrepExec 1\n");
	if (HandleExecFd(GetFdTable(GetProcess()))) {
		LogWriteSerial("SysPrepExec E1\n");
		return EINVAL;
	}
	LogWriteSerial("SysPrepExec 2\n");
	if (WipeUsermodePages()) {
		LogWriteSerial("SysPrepExec E2\n");
		return ENOTRECOVERABLE;
	}
	LogWriteSerial("SysPrepExec 3\n");
	// TODO: we need to reset the allocator that gives us AllocVirtRange
	// (we can just completely nuke it and make it seem like it was a clean boot).
	return 0;
}
