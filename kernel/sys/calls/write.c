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

	// 0x0 0x19 0xFFFFFD3 0xFFFFFCC 0x0

	if (file == NULL || res != 0) {
		LogWriteSerial("SysWrite failed [A]: %d\n", res);
		return res;
	}

	uint8_t* userptr_buffer = (uint8_t*) buffer_;
	size_t* userptr_br_out = (size_t*) br_out_;

	if (file->flags & O_APPEND) {
		struct stat st;
        res = VnodeOpStat(file->node, &st);
        if (res != 0) {
			LogWriteSerial("SysWrite failed [B]: %d\n", res);
            return res;
        }

        file->seek_position = st.st_size;
	}

	LogWriteSerial("SysWrite: fd = %d, openfile = 0x%X, node = 0x%X\n", fd, file, file->node);

	struct transfer io = CreateTransferReadingFromUser(userptr_buffer, size, file->seek_position);
	res = WriteFile(file, &io);
	if (res != 0) {
		LogWriteSerial("SysWrite failed [C]: %d\n", res);
		return res;
	}

	size_t br = size - io.length_remaining;
	file->seek_position += br;
	
	res = WriteWordToUsermode(userptr_br_out, br);
	if (res != 0) {
		LogWriteSerial("SysWrite failed [D]: %d\n", res);
	}
	return res;
}
