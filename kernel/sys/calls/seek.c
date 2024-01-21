#include <syscall.h>
#include <errno.h>
#include <_syscallnum.h>
#include <thread.h>
#include <log.h>
#include <vfs.h>
#include <process.h>
#include <dirent.h>
#include <filedes.h>
#include <unistd.h>
#include <transfer.h>

int SysSeek(size_t fd, size_t pos_ptr, size_t whence, size_t, size_t) {    
	struct file* file;
	int res = GetFileFromFd(GetFdTable(GetProcess()), fd, &file);

	if (file == NULL || res != 0) {
		return res;
	}

	struct transfer io = CreateTransferReadingFromUser((void*) pos_ptr, sizeof(off_t), 0);
    off_t offset;
    if ((res = PerformTransfer(&offset, &io, sizeof(off_t)))) {
        return res;
    }

    int type = VnodeOpDirentType(file->node);
    if (type == DT_FIFO || type == DT_SOCK) {
        return ESPIPE;
    }
    
    if (whence == SEEK_CUR) {
        offset += file->seek_position;

    } else if (whence == SEEK_END) {
        offset += file->node->stat.st_size;

    } else if (whence != SEEK_SET) {
        return EINVAL;
    }

    file->seek_position = offset;

    io = CreateTransferWritingToUser((void*) pos_ptr, sizeof(off_t), 0);
    return PerformTransfer(&offset, &io, sizeof(off_t));
}
