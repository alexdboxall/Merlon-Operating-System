#include <syscall.h>
#include <errno.h>
#include <_syscallnum.h>
#include <thread.h>
#include <log.h>
#include <vfs.h>
#include <transfer.h>
#include <fcntl.h>
#include <process.h>
#include <filedes.h>

int SysIoctl(size_t fd, size_t cmd, size_t arg1, size_t, size_t success_retv) {
	
	struct fd_table* table = GetFdTable(GetProcess());
	struct file* file;
	int res = GetFileFromFd(table, fd, &file);
	if (res != 0) {
		return res;
	}

	LogWriteSerial("SysIoctl: [0x%X, 0x%X] %d, %d, 0x%X\n", file, file->node, fd, cmd, arg1);

	res = VnodeOpIoctl(file->node, cmd, (void*) arg1);
	if (res != 0) {
		return res;
	} else {
		return WriteWordToUsermode((size_t*) success_retv, 0);
	}
}
