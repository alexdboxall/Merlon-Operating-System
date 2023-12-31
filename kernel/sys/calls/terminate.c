#include <syscall.h>
#include <errno.h>
#include <_syscallnum.h>
#include <thread.h>

int SysTerminate(size_t, size_t, size_t, size_t, size_t) {
	TerminateThread(GetThread());
	return EFAULT;
}
