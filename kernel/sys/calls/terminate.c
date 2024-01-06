#include <syscall.h>
#include <errno.h>
#include <_syscallnum.h>
#include <thread.h>
#include <process.h>
#include <assert.h>

int SysTerminate(size_t status, size_t, size_t, size_t, size_t) {
	assert(GetProcess() != NULL);

	// TODO: check if last thread in process
	// if (GetNumberOfRemainingThreads(GetProcess()) == 1) {
	//     KillProcess(status);
	// } else {
	//     TerminateThread(GetThread());
	// }
	
	(void) status;
	
	TerminateThread(GetThread());
	return EFAULT;
}
