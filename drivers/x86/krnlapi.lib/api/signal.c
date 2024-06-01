
#include <signal.h>
#include <errno.h>
#include "krnlapi.h"

extern void x86UserHandleSignals(void);

static void SignalActionTerminate(int sig_num) {
    dbgprintf("Terminated due to signal %d.\n", sig_num);
    _system_call(SYSCALL_EXIT, sig_num, 0, 0, 0, 0);
}

static void SignalActionCore(int sig_num) {
    dbgprintf("<< TODO: CORE DUMP >>\n");
    SignalActionTerminate(sig_num);
}

static void SignalDefault(int sig_num) {
    switch (sig_num) {
    case SIGABRT:
    case SIGBUS:
    case SIGFPE:
    case SIGILL:
    case SIGQUIT:
    case SIGSEGV:
    case SIGSYS:
    case SIGTRAP:
    case SIGXCPU:
    case SIGXFSZ:
        SignalActionCore(sig_num);
        return;
    case SIGALRM:
    case SIGHUP:
    case SIGINT:
    case SIGKILL:
    case SIGPIPE:
    case SIGPOLL:
    case SIGPROF:
    case SIGTERM:
    case SIGUSR1:
    case SIGUSR2:
    case SIGVTALRM:
        SignalActionTerminate(sig_num);
        return;
    default:
        return;
    }
}

static void SignalIgnore(int sig_num) {
    (void) sig_num;
}

/*
 * They should all be set to `SignalDefault`, but we'll treat NULL as being 
 * a placeholder meaning SignalDefault. 
 */
void (*signal_handers[64])(int) = {0};

int OsCommonSignalHandler(int sig_num) {
    dbgprintf("Got signal %d\n", sig_num);
    if (sig_num < 64) {
        if (signal_handers[sig_num] == NULL) {
            signal_handers[sig_num] = SignalDefault;
        }
        signal_handers[sig_num](sig_num);
    }
    _system_call(SYSCALL_SIGNAL, 1, 0, sig_num, 0, 0);
    return 0;
}

void OsInitSignals(void) {
    _system_call(SYSCALL_SIGNAL, 0, (size_t) x86UserHandleSignals, 0, 0, 0);
}

void (*signal(int sig, void (*func)(int)))(int) {
    if (func == SIG_DFL) {
        func = SignalDefault;
    }
    if (func == SIG_IGN) {
        func = SignalIgnore;
    }
    if (sig < 64) {
        void (*old_handler)(int) = signal_handers[sig];
        signal_handers[sig] = func;
        if (old_handler == NULL) {
            old_handler = SignalDefault;
        }
        return old_handler;
    } else {
        errno = EINVAL;
        return SIG_ERR;
    }
}

int raise(int sig) {
    (void) sig;
    return ENOSYS;
}

int kill(pid_t pid, int sig) {
    (void) pid;
    (void) sig;
    errno = ENOSYS;
    return -1;
}