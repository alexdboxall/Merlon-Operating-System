#include <syscall.h>
#include <errno.h>
#include <_syscallnum.h>
#include <log.h>
#include <thread.h>
#include <process.h>

int SysFork(size_t pidout, size_t, size_t, size_t, size_t) {
	LogWriteSerial("ABOUT TO ForkProcess()!\n");
	struct process* new_prcss = ForkProcess();
	(void) new_prcss;
	(void) pidout;
	LogWriteSerial("ForkProcess() RETRUNED!\n");
	return 0;
}
