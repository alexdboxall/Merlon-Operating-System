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

int SysTell(size_t fd, size_t pos_ptr_, size_t, size_t, size_t) {    
    struct filedes_table* table = GetFileDescriptorTable(GetProcess());
	struct open_file* file;
	int res = GetFileFromDescriptor(table, fd, &file);

	if (file == NULL || res != 0) {
		return res;
	}

	struct transfer io = CreateTransferReadingFromUser((void*) pos_ptr_, sizeof(off_t), 0);
    off_t offset;
    res = PerformTransfer(&offset, &io, sizeof(off_t));
    if (res != 0) {
        return res;
    }

    int type = VnodeOpDirentType(file->node);
    if (type == DT_FIFO || type == DT_SOCK) {
        return ESPIPE;
    }

    offset = file->seek_position;

    io = CreateTransferWritingToUser((void*) pos_ptr_, sizeof(off_t), 0);
    return PerformTransfer(&offset, &io, sizeof(off_t));
}
