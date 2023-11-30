#pragma once

#include <common.h>

export uint64_t GetSystemTimer(void);

void ReceivedTimer(uint64_t nanos);
void InitTimer(void);

/*
 * Internal functions to do shenanigans
 */
struct thread;
void QueueForSleep(struct thread* thr);
bool TryDequeueForSleep(struct thread* thr);