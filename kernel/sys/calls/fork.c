#include <syscall.h>
#include <errno.h>
#include <_syscallnum.h>
#include <log.h>
#include <thread.h>
#include <transfer.h>
#include <process.h>

int SysFork(size_t pidout, size_t user_stub_addr, size_t, size_t, size_t) {
	LogWriteSerial("ABOUT TO ForkProcess()!\n");
	struct process* new_prcss = ForkProcess(user_stub_addr);
	LogWriteSerial("ForkProcess() RETRUNED!\n");

	struct transfer io;
    io = CreateTransferWritingToUser((void*) pidout, sizeof(pid_t), 0);
    return PerformTransfer(&new_prcss->pid, &io, sizeof(pid_t));
}
