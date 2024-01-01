#include <syscall.h>
#include <errno.h>
#include <_syscallnum.h>
#include <thread.h>
#include <log.h>
#include <vfs.h>
#include <process.h>
#include <filedes.h>

int SysClose(size_t fd, size_t, size_t, size_t, size_t) {
	struct filedes_table* table = GetFileDescriptorTable(GetProcess());
	struct open_file* file;
	int res = GetFileFromDescriptor(table, fd, &file);

	if (file == NULL || res != 0) {
		return res;
	}

	return CloseFile(file);
}
