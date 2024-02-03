#include <syscall.h>
#include <errno.h>
#include <_syscallnum.h>
#include <thread.h>
#include <process.h>

int SysExit(size_t status, size_t, size_t, size_t, size_t) {
	KillProcess(status);
	return ENOTRECOVERABLE;
}
