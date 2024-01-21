#include <syscall.h>
#include <errno.h>
#include <_syscallnum.h>
#include <thread.h>
#include <process.h>
#include <virtual.h>
#include <filedes.h>
#include <log.h>

int SysPrepExec(size_t, size_t, size_t, size_t, size_t) {
	if (HandleExecFd(GetFdTable(GetProcess()))) {
		return EINVAL;
	}
	if (WipeUsermodePages()) {
		return EUNRECOVERABLE;
	}
	// TODO: we need to reset the allocator that gives us AllocVirtRange
	// (we can just completely nuke it and make it seem like it was a clean boot).
	return 0;
}
