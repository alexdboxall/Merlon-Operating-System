#pragma once

#include <common.h>

struct thread;

void RaiseSignal(struct thread* thr, int sig_num, bool lock_already_held);
int FindSignalToHandle();
void FinishHandlingSignal(void);