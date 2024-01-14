#include <syscall.h>
#include <errno.h>
#include <_syscallnum.h>
#include <thread.h>
#include <log.h>
#include <vfs.h>
#include <process.h>
#include <filedes.h>

int SysWaitpid(size_t pid, size_t pidout, size_t statusout, size_t options, size_t) {
	int status;
	pid_t out = WaitProcess(pid, &status, options);
	
	struct transfer io = CreateTransferWritingToUser((void*) pidout, sizeof(pid_t), 0);
    PerformTransfer(&out, &io, sizeof(pid_t));
    if (io.length_remaining != 0) {
        return EINVAL;
    }

	io = CreateTransferWritingToUser((void*) statusout, sizeof(int), 0);
    PerformTransfer(&status, &io, sizeof(int));
    if (io.length_remaining != 0) {
        return EINVAL;
    }

	return 0;
}
