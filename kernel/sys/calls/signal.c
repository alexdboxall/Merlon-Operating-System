#include <syscall.h>
#include <errno.h>
#include <transfer.h>
#include <arch.h>
#include <_syscallnum.h>
#include <string.h>
#include <ksignal.h>
#include <signal.h>
#include <thread.h>

int SysSignal(size_t op, size_t ptr_arg, size_t sig_num, size_t, size_t) {
    struct thread* thr = GetThread();
    if (op == 0) {
        if (thr->user_common_signal_handler == 0) {
            thr->user_common_signal_handler = ptr_arg;
            return 0;
        } else {
            return EALREADY;
        }

    } else if (op == 1) {
        thr->blocked_signals &= ~(1 << sig_num);
        return 0;

    } else {
        return EINVAL;
    }
}