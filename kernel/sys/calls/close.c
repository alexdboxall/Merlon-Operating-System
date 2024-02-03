#include <syscall.h>
#include <thread.h>
#include <vfs.h>
#include <process.h>
#include <filedes.h>

int SysClose(size_t fd, size_t, size_t, size_t, size_t) {
	struct fd_table* table = GetFdTable(GetProcess());
	struct file* file;
	int res = GetFileFromFd(table, fd, &file);
	if (res != 0) {
		return res;
	}
	RemoveFd(table, file);
	return CloseFile(file);
}
