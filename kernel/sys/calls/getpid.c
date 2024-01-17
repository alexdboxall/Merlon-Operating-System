#include <syscall.h>
#include <errno.h>
#include <_syscallnum.h>
#include <thread.h>
#include <log.h>
#include <vfs.h>
#include <process.h>
#include <filedes.h>

int SysGetPid(size_t pidout, size_t get_ppid, size_t, size_t, size_t) {
    struct process* proc = GetProcess();
    
    pid_t pid = 0;
    if (proc != NULL) {
        pid = (get_ppid == 0) ? proc->pid : proc->parent;
    } 
    
	struct transfer io;
    io = CreateTransferWritingToUser((void*) pidout, sizeof(pid_t), 0);
    PerformTransfer(&pid, &io, sizeof(pid_t));
	return 0;
}
