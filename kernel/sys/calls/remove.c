#include <syscall.h>
#include <errno.h>
#include <_syscallnum.h>
#include <thread.h>
#include <transfer.h>
#include <log.h>
#include <fcntl.h>
#include <thread.h>
#include <process.h>
#include <vfs.h>
#include <filedes.h>

int SysRemove(size_t filename, size_t rmdir, size_t, size_t, size_t) {
	if (rmdir > 1) {
		return EINVAL;
	}

	char path[400];
	int res = ReadStringFromUsermode(path, (const char*) filename, 399);
	if (res != 0) {
		return res;
	}
	return RemoveFileOrDirectory(path, rmdir);
}
