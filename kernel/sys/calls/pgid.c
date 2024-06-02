#include <syscall.h>
#include <errno.h>
#include <_syscallnum.h>
#include <thread.h>
#include <log.h>
#include <vfs.h>
#include <process.h>
#include <filedes.h>

/*
pid_t getpgid(pid_t pid) {
    pid_t out;
    int res = _system_call(SYSCALL_PGID, 0, (size_t) &pid, (size_t) &out, 0, 0);
    if (res != 0) {
        errno = res;
        return -1;
    }
    return out;
}

int setpgid(pid_t pid, pid_t pgid) {
    int res = _system_call(SYSCALL_PGID, 1, (size_t) &pid, (size_t) &pgid, 0, 0);
    if (res != 0) {
        errno = res;
        return -1;
    }
    return out;
}
*/

int SysPgid(size_t op, size_t pid_ptr, size_t arg_ptr, size_t, size_t) {
    pid_t pid;

    struct transfer io;
    io = CreateTransferReadingFromUser((void*) pid_ptr, sizeof(pid_t), 0);
    int res = PerformTransfer(&pid, &io, sizeof(pid_t));
    if (res != 0) {
        return res;
    }

    struct process* prcss = GetProcessFromPid(pid);
    if (prcss == NULL) {
        return EINVAL;
    }

    if (op == 0) {
        /* getpgid() */
        pid_t pgid = prcss->pgid;
        io = CreateTransferWritingToUser((void*) pid_ptr, sizeof(pid_t), 0);
        return PerformTransfer(&pgid, &io, sizeof(pid_t));

    } else if (op == 1) {
        /* setpgid() */
        pid_t pgid;
        io = CreateTransferReadingFromUser((void*) arg_ptr, sizeof(pid_t), 0);
        res = PerformTransfer(&pgid, &io, sizeof(pid_t));
        if (res != 0) {
            return res;
        }

        prcss->pgid = pgid;
        return 0;
    
    } else {
        return EINVAL;
    }
}
