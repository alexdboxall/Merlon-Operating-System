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
#include <thread.h>
#include <timer.h>

int SysNanosleep(size_t wait_ptr, size_t remain_ptr, size_t, size_t, size_t) {
	struct transfer io;
	uint64_t wait_ns;
    io = CreateTransferReadingFromUser((void*) wait_ptr, sizeof(uint64_t), 0);
    int res = PerformTransfer(&wait_ns, &io, sizeof(uint64_t));
	if (res != 0) {
		return EFAULT;
	}

	/*
	 * TODO: need to allow this to be EINTR'ed.
	 */
	uint64_t start_time = GetSystemTimer();
	SleepNano(wait_ns);

	int64_t remain_ns = GetSystemTimer() - start_time;
	if (remain_ns < 0) {
		remain_ns = 0;
	}
	uint64_t remain_ns_unsigned = remain_ns;
	io = CreateTransferWritingToUser((void*) remain_ptr, sizeof(uint64_t), 0);
    return PerformTransfer(&remain_ns_unsigned, &io, sizeof(uint64_t));
}

