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

#define ALLOWABLE_FLAGS (O_CREAT | O_EXCL | O_NOCTTY | O_TRUNC | O_APPEND \
						| O_NONBLOCK | O_CLOEXEC | O_DIRECT | O_ACCMODE)

int SysOpen(size_t filename, size_t flags, size_t mode, size_t fdout, size_t) {
	char path[400];
	int fd;
	int res;

	if (flags & ~ALLOWABLE_FLAGS) {
		return EINVAL;
	}

	if ((res = ReadStringFromUsermode(path, (const char*) filename, 399))) {
		return res;
	}

	struct file* file;	
	if ((res = OpenFile(path, flags, mode, &file))) {
		return res;
	}

	struct fd_table* table = GetFdTable(GetProcess());
	if ((res = CreateFd(table, file, &fd, flags & O_CLOEXEC))) {
		CloseFile(file);
		return res;
	}	

	if ((res = WriteWordToUsermode((size_t*) fdout, fd))) {
		CloseFile(file);
		RemoveFd(table, file);
		return res;
	} 

	return 0;
}
