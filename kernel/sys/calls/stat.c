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

int SysStat(size_t filename, size_t out, size_t use_fd, size_t fd, size_t symlink) {
	char path[400];
	int res;
	struct file* file;

	if (use_fd != 0) {
		if (symlink != 0) {
			return EINVAL;
		}
		struct fd_table* table = GetFdTable(GetProcess());
		int res = GetFileFromFd(table, fd, &file);

		if (file == NULL || res != 0) {
			return res;
		}

		struct transfer tr = CreateTransferWritingToUser((void*) out, sizeof(struct stat), 0);
		return PerformTransfer(&file->node->stat, &tr, sizeof(struct stat));

	} else {
		if (symlink == 1) {
			return ENOSYS;
		} else if (symlink > 1) {
			return EINVAL;
		}
		
		if ((res = ReadStringFromUsermode(path, (const char*) filename, 399))) {
			return res;
		}

		if ((res = OpenFile(path, O_RDONLY, 0, &file))) {
			return res;
		}

		struct transfer tr = CreateTransferWritingToUser((void*) out, sizeof(struct stat), 0);
		res = PerformTransfer(&file->node->stat, &tr, sizeof(struct stat));
		CloseFile(file);
		return res;
	}
}
