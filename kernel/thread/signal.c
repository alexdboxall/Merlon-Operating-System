
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

int RaiseSignal(struct thread* thr, int sig_num, bool lock_already_held) {
    if (sig_num >= _SIG_UPPER_BND) {
        return EINVAL;
    }
    if (!lock_already_held) {
        LockScheduler();
    }
    thr->signal_intr = true;
    thr->pending_signals |= 1 << sig_num;
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
    uint32_t available_signals = thr->pending_signals & (~thr->blocked_signals);
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
    struct thread* thr = GetThread();
    if (sig_num == SIGKILL) {
        thr->needs_termination = true;
        return 0;
    }

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