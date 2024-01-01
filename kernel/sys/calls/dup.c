#include <syscall.h>
#include <errno.h>
#include <_syscallnum.h>
#include <thread.h>
#include <log.h>
#include <vfs.h>
#include <process.h>
#include <filedes.h>

int SysDup(size_t dup_num, size_t old_fd, size_t new_fd, size_t flags, size_t) {
	struct filedes_table* table = GetFileDescriptorTable(GetProcess());

	if (dup_num == 1) {
		int result_fd;
		int res = DuplicateFileDescriptor(table, old_fd, &result_fd);
		if (res != 0) {
			return res;
		}

		return WriteWordToUsermode((size_t*) new_fd, result_fd);

	} else if (dup_num == 2) {
		return DuplicateFileDescriptor2(table, old_fd, new_fd);
		
	} else if (dup_num == 3) {
		return DuplicateFileDescriptor3(table, old_fd, new_fd, flags);

	} else {
		return EINVAL;
	}
}
