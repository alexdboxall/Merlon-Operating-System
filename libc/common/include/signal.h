#pragma once

#include <stddef.h>
#include <sys/types.h>

typedef uint32_t sigset_t;

/*
 * SIG_DFL must be 0, as we memset() the default signal structure to zero.
 */
#define SIG_DFL         ((void (*)(int)) (size_t) 0)
#define SIG_IGN         ((void (*)(int)) (size_t) 1)

#define SIG_ERR         ((void (*)(int)) (size_t) -1)

#define SIGABRT         1
#define SIGALRM         2
#define SIGBUS          3
#define SIGCHLD         4
#define SIGCONT         5
#define SIGFPE          6
#define SIGHUP          7
#define SIGILL          8
#define SIGKILL         9
#define SIGINT          10
#define SIGPIPE         11
#define SIGQUIT         12
#define SIGSEGV         13
#define SIGSTOP         14
#define SIGTERM         15
#define SIGTSTP         16
#define SIGTTIN         17
#define SIGTTOU         18
#define SIGUSR1         19
#define SIGUSR2         20
#define SIGPOLL         21
#define SIGPROF         22
#define SIGSYS          23
#define SIGTRAP         24
#define SIGURG          25
#define SIGVTALRM       26
#define SIGXCPU         27
#define SIGXFSZ         28
#define _SIG_UPPER_BND  29  // MUST BE LESS THAN 32 (i.e. 31 is MAX)
                            // AS WE USE UINT32_T AT THE MOMENT FOR HOLDING
                            // SIGNAL INFO (to speed up `FindSignalToHandle`,
                            // which runs on every IRQ)
                            
#define SIG_BLOCK   1
#define SIG_UNBLOCK 2
#define SIG_SETMASK 3

void (*signal(int sig, void (*func)(int)))(int);
int raise(int sig);
int kill(pid_t pid, int sig);

int sigprocmask(int how, const sigset_t* restrict set, sigset_t* restrict oldset);
int sigsuspend(const sigset_t* mask);

int sigemptyset(sigset_t *set);
int sigfillset(sigset_t *set);
int sigaddset(sigset_t *set, int signum);
int sigdelset(sigset_t *set, int signum);
int sigismember(const sigset_t *set, int signum);