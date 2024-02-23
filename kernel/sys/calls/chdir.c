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

int SysChdir(size_t fd, size_t, size_t, size_t, size_t) {
	struct fd_table* table = GetFdTable(GetProcess());
	struct file* file;
	int res = GetFileFromFd(table, fd, &file);
	if (res != 0) {
		return res;
	}

	return SetWorkingDirectory(file->node);
}
