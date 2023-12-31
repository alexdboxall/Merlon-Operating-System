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

int SysOpen(size_t filename_, size_t flags, size_t mode, size_t fdout_, size_t) {
	const char* userptr_filename = (const char*) filename_;
	size_t* userptr_fdout = (size_t*) fdout_;

	char path[400];
	path[399] = 0;
	int res = ReadStringFromUsermode(path, userptr_filename, 399);
	if (res != 0) {
		return res;
	}

	struct open_file* file;
	res = OpenFile(path, flags, mode, &file);
	
	if (res != 0) {
		return res;
	}

	int fd;
	res = CreateFileDescriptor(GetFileDescriptorTable(GetProcess()), file, &fd, flags & FD_CLOEXEC);
	if (res != 0) {
		CloseFile(file);
		return res;
	}	

	res = WriteWordToUsermode(userptr_fdout, fd);
	if (res != 0) {
		CloseFile(file);
		RemoveFileDescriptor(GetFileDescriptorTable(GetProcess()), file);
		return res;
	} 

	return 0;
}
