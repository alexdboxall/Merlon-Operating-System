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
void DequeueForSleep(struct thread* thr);