#include <syscall.h>
#include <errno.h>
#include <_syscallnum.h>
#include <thread.h>
#include <process.h>

int SysFork(size_t pid_out, size_t, size_t, size_t, size_t) {
	(void) pid_out;
	return ENOSYS;
}
