#include <syscall.h>
#include <errno.h>
#include <_syscallnum.h>
#include <thread.h>
#include <log.h>

int SysYield(size_t, size_t, size_t, size_t, size_t) {
	Schedule();
	return 0;
}
