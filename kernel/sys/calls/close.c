#include <syscall.h>
#include <thread.h>
#include <vfs.h>
#include <process.h>
#include <filedes.h>

int SysClose(size_t fd, size_t, size_t, size_t, size_t) {
	struct file* file;
	int res = GetFileFromFd(GetFileFromFdDescriptorTable(GetProcess()), fd, &file);
	if (res != 0) {
		return res;
	}
	return CloseFile(file);
}
