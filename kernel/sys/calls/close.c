#include <syscall.h>
#include <errno.h>
#include <_syscallnum.h>
#include <thread.h>
#include <log.h>
#include <vfs.h>
#include <process.h>
#include <filedes.h>

int SysClose(size_t fd, size_t, size_t, size_t, size_t) {
	struct open_file* file;
	int res;

	if ((res = GetFileFromDescriptor(GetFileDescriptorTable(GetProcess()), fd, &file))) {
		return res;
	}

	return CloseFile(file);
}
