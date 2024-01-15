#include <syscall.h>
#include <errno.h>
#include <_syscallnum.h>
#include <thread.h>
#include <log.h>
#include <vfs.h>
#include <process.h>
#include <filedes.h>
#include <fcntl.h>

int SysDup(size_t dup_num, size_t old_fd, size_t new_fd, size_t flags, size_t) {
	struct filedes_table* table = GetFileDescriptorTable(GetProcess());

	if ((flags & ~O_CLOEXEC) != 0) {
		return EINVAL;
	}

	if (dup_num == 1) {
		int result_fd;
		int res = DupFd(table, old_fd, &result_fd);
		if (res != 0) {
			return res;
		}

		return WriteWordToUsermode((size_t*) new_fd, result_fd);

	} else if (dup_num == 2) {
		return DupFd2(table, old_fd, new_fd, flags);
		
	} else {
		return EINVAL;
	}
}
