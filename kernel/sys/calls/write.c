#include <syscall.h>
#include <errno.h>
#include <_syscallnum.h>
#include <thread.h>
#include <log.h>
#include <vfs.h>
#include <transfer.h>
#include <fcntl.h>
#include <process.h>
#include <filedes.h>

int SysWrite(size_t fd, size_t size, size_t buffer_, size_t br_out_, size_t) {
	struct filedes_table* table = GetFileDescriptorTable(GetProcess());
	struct open_file* file;
	int res = GetFileFromDescriptor(table, fd, &file);

	if (file == NULL || res != 0) {
		return res;
	}

	uint8_t* userptr_buffer = (uint8_t*) buffer_;
	size_t* userptr_br_out = (size_t*) br_out_;

	if (file->flags & O_APPEND) {
		struct stat st;
        res = VnodeOpStat(file->node, &st);
        if (res != 0) {
            return res;
        }

        file->seek_position = st.st_size;
	}

	struct transfer io = CreateTransferReadingFromUser(userptr_buffer, size, file->seek_position);
	res = WriteFile(file, &io);
	if (res != 0) {
		return res;
	}

	size_t br = size - io.length_remaining;
	file->seek_position += br;
	
	return WriteWordToUsermode(userptr_br_out, br);
}
