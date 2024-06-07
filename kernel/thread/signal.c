
#include <cpu.h>
#include <thread.h>
#include <spinlock.h>
#include <heap.h>
#include <assert.h>
#include <string.h>
#include <irql.h>
#include <log.h>
#include <errno.h>
#include <panic.h>
#include <common.h>
#include <semaphore.h>
#include <process.h>
#include <ksignal.h>
#include <signal.h>
#include <tree.h>

void StopThread(struct thread* thr) {
    LockScheduler();
    if (thr == GetThread()) {
        thr->needs_stopping = false;
        BlockThread(THREAD_STATE_STOPPED);
    } else {
        thr->needs_stopping = true;
    }
    UnlockScheduler();
}

void RaiseSignalToProcessGroup(pid_t pgid, int sig_num) {
    struct linked_list* prcsses = GetProcessesFromPgid(pgid);
    struct linked_list_node* node = ListGetFirstNode(prcsses);

    LockScheduler();
    while (node != NULL) {
        struct process* p = ListGetDataFromNode(node);
        RaiseSignal(GetArbitraryThreadFromProcess(p), sig_num, true);
        node = ListGetNextNode(node);
    }
    UnlockScheduler();
}

int RaiseSignal(struct thread* thr, int sig_num, bool lock_already_held) {
    if (sig_num >= _SIG_UPPER_BND) {
        return EINVAL;
    }
    if (!lock_already_held) {
        LockScheduler();
    }
    thr->signal_intr = true;
    thr->pending_signals |= 1 << sig_num;

    /*
     * We must handle these special signals here, as they must be handled, and,
     * e.g. a stopped thread would not be able to (as `HandleSignal` runs as the
     * current thread.
     */
    if (sig_num == SIGKILL) {
        thr->needs_termination = true;

    } else if (sig_num == SIGSTOP) {
        thr->needs_stopping = true;
        
    } else if (sig_num == SIGCONT) {
        if (thr->state == THREAD_STATE_STOPPED) {
            UnblockThread(thr);
        }
    }

    if (thr->state == THREAD_STATE_WAITING_FOR_SIGNAL) {
        /*
         * TODO: this should probably check if the signal is unblocked in `thr`
         */
        UnblockThread(thr);
    }

    if (!lock_already_held) {
        UnlockScheduler();
    }

    return 0;
}

bool HasBeenSignalled(void) {
    return GetThread()->signal_intr;
}

/**
 * Should be called on task switches to detect if a signal needs to be handled.
 * If it does, it blocks the signal, clears its pending bit, and returns the
 * signal number to handle.
 * 
 * Returns -1 if there is no signals that can be handled.
 */
int FindSignalToHandle() {
    /*
     * The signal_intr bit stops blocking tasks such as the disk from running,
     * and because we have reached this far, we're already about to handle the
     * signal so no need to continue stopping further operations.
     */
    struct thread* thr = GetThread();
    if (thr == NULL) {
        return -1;
    }
    sigset_t available_signals = thr->pending_signals & (~thr->blocked_signals);
    if (available_signals == 0) {
        return -1;
    }

    thr->signal_intr = false;

    int index = 0;
    while (!(available_signals & 1)) {
        available_signals >>= 1;
        ++index;
    }

    thr->pending_signals &= ~(1 << index);
    thr->blocked_signals |= 1 << index;
    return index;
}

size_t HandleSignal(int sig_num) {
    /*
     * SIGSTOP, SIGKILL and SIGCONT are handled in the sending side of a signal,
     * as `HandleSignal` runs as the current thread, and e.g. a stopped thread
     * can't respond to signals (e.g. to receive SIGCONT or SIGKILL).
     */

    struct thread* thr = GetThread();
    if (thr->user_common_signal_handler == 0) {
        thr->needs_termination = true;
    }

    /*
     * If the signal ran, it can't have been blocked by the user, and hence it
     * was blocked by `FindSignalToHandle`, so revert that.
     */
    thr->blocked_signals &= ~(1 << sig_num);
    return thr->user_common_signal_handler;
}

static void ProtectSpecialSignals(sigset_t* blocked) {
    *blocked &= ~((1 << SIGKILL) | (1 << SIGSTOP) | (1 << SIGCONT));
}

/*
 * Can be used to implement `sigsuspend()`.
 */
int SuspendForSignal(sigset_t new_mask, sigset_t* old_mask, bool protect) {
    if (old_mask == NULL) {
        return EFAULT;
    }

    LockScheduler();
    struct thread* thr = GetThread();
    *old_mask = thr->blocked_signals;
    thr->prev_blocked_signals = thr->blocked_signals;
    thr->blocked_signals = new_mask;
    if (protect) {
        ProtectSpecialSignals(&thr->blocked_signals);
    }
    BlockThread(THREAD_STATE_WAITING_FOR_SIGNAL);
    UnlockScheduler();
    Schedule();
    LockScheduler();
    thr->blocked_signals = thr->prev_blocked_signals;
    UnlockScheduler();
    return EINTR;
}

int PauseForSignal(void) {
    LockScheduler();
    BlockThread(THREAD_STATE_WAITING_FOR_SIGNAL);
    UnlockScheduler();
    return EINTR;
}

/*
 * `sigprocmask()`
 */
int SetBlockedSignals(int how, sigset_t* changes, sigset_t* old, bool protect) {
    if (changes == NULL) {
        return EFAULT;
    }

    int retv = 0;
    struct thread* thr = GetThread();

    LockScheduler();

    if (old != NULL) {
        *old = thr->blocked_signals;
    }

    if (how == SIG_BLOCK) {
        thr->blocked_signals |= *changes;
    } else if (how == SIG_UNBLOCK) {
        thr->blocked_signals &= ~(*changes);
    } else if (how == SIG_SETMASK) {
        thr->blocked_signals = *changes;
    } else {
        retv = EINVAL;
    }

    if (protect) {
        ProtectSpecialSignals(&thr->blocked_signals);
    }

    UnlockScheduler();

    return retv;
}