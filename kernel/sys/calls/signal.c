#include <syscall.h>
#include <errno.h>
#include <transfer.h>
#include <arch.h>
#include <_syscallnum.h>
#include <string.h>
#include <ksignal.h>
#include <signal.h>
#include <thread.h>
#include <tree.h>
#include <process.h>

/*
 * TODO: should split up into:
 *
 * SysSignalInternal (op 0, 1)
 * SysKill (op 2)
 * SysSigsuspend (op 3)
 * SysSigprocmask (op 4)
 * 
 * Also make the args less arcane...
 */
int SysSignal(size_t op, size_t ptr_arg, size_t sig_num, size_t arg, size_t) {
    struct thread* thr = GetThread();
    if (op == 0) {
        /*
         * This call installs the common signal handler.
         */
        if (thr->user_common_signal_handler == 0) {
            thr->user_common_signal_handler = ptr_arg;
            return 0;
        } else {
            return EALREADY;
        }

    } else if (op == 1) {
        /*
         * This call is the "end of signal" call - i.e. sigreturn
         */

        if (sig_num >= _SIG_UPPER_BND) {
            /*
             * Prevent dodgy user processes from trying to muck up the internal
             * blocked signal state.
             */
            return EINVAL;
        }
        thr->blocked_signals &= ~(1 << sig_num);
        return 0;

    } else if (op == 2) {
        /*
         * This is the kill(2) call.
         */
        pid_t pid = arg;
        if (pid > 0) {
            struct process* prcss = GetProcessFromPid(arg);
            if (prcss == NULL) {
                return EINVAL;
            }

            /*
             * "According to POSIX.1, a process-directed signal (sent using 
             *  kill(2), for example) should be handled by a single, arbitrarily
             *  selected thread within the process."
             */
            struct thread* thr = prcss->threads->root->data;
            if (thr == NULL) {
                return EINVAL;
            }

            return RaiseSignal(thr, sig_num, false);

        } else if (pid == 0) {
            // ...
            
        } else if (pid < 0) {
            // ...
        }
        return ENOSYS;

    } else if (op == 3) {
        /*
         * This is the `sigsuspend` call.
         */

        sigset_t new_mask;
        sigset_t old_mask;
        struct transfer io;
        int res;

        io = CreateTransferReadingFromUser((void*) ptr_arg, sizeof(sigset_t), 0);
        res = PerformTransfer(&new_mask, &io, sizeof(sigset_t));
        if (res != 0) {
            return res;
        }

        res = SuspendForSignal(new_mask, &old_mask, true);
        if (res != EINTR) {
            return res;
        }

        io = CreateTransferWritingToUser((void*) arg, sizeof(sigset_t), 0);
        return PerformTransfer(&old_mask, &io, sizeof(sigset_t));

    } else if (op == 4) {
        /*
         * `sigprocmask`
         */
        sigset_t old_mask;
        sigset_t changes;
        struct transfer io;
        int res;

        io = CreateTransferReadingFromUser((void*) ptr_arg, sizeof(sigset_t), 0);
        res = PerformTransfer(&changes, &io, sizeof(sigset_t));
        if (res != 0) {
            return res;
        }

        res = SetSignalProtectionMask(sig_num, &changes, &old_mask, true);
    
        io = CreateTransferWritingToUser((void*) arg, sizeof(sigset_t), 0);
        return PerformTransfer(&old_mask, &io, sizeof(sigset_t));

    } else {
        return EINVAL;
    }
}