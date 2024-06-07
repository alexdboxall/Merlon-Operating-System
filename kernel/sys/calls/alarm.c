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
#include <timer.h>

int SysAlarm(size_t in, size_t out, size_t, size_t, size_t) {
	uint64_t remaining_us;
	uint64_t alarm_us;
	int res;

	struct transfer io;
	io = CreateTransferReadingFromUser((void*) in, sizeof(uint64_t), 0);
    res = PerformTransfer(&alarm_us, &io, sizeof(uint64_t));
	if (res != 0) {
		return res;
	}

	res = InstallUnixAlarm(alarm_us, &remaining_us);
	if (res != 0) {
		return res;
	}

    io = CreateTransferWritingToUser((void*) out, sizeof(uint64_t), 0);
    return PerformTransfer(&remaining_us, &io, sizeof(uint64_t));
}
