#pragma once

#include <common.h>

struct thread;

int RaiseSignal(struct thread* thr, int sig_num, bool lock_already_held);
void RaiseSignalToProcessGroup(pid_t pgid, int sig_num);

int FindSignalToHandle();
size_t HandleSignal(int sig_num);

#include <signal.h>

int SetBlockedSignals(int how, sigset_t* changes, sigset_t* old, bool protect);
int SuspendForSignal(sigset_t new_mask, sigset_t* old_mask, bool protect);
int PauseForSignal(void);