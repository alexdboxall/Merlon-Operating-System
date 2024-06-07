#pragma once

#include <common.h>

uint64_t GetSystemTimer(void);

void ReceivedTimer(uint64_t nanos);
void InitTimer(void);

int CreateAlarmAbsolute(uint64_t system_time_ns, void (*callback)(void*), void* arg, int* id_out);
int CreateAlarmMicro(uint64_t delta_us, void (*callback)(void*), void* arg, int* id_out);
int GetAlarmTimeRemaining(int id, uint64_t* time_left_out);
int DestroyAlarm(int id, uint64_t* time_left_out);
int InstallUnixAlarm(uint64_t microseconds, uint64_t* remaining_microsecs);

/*
 * Internal functions to do shenanigans
 */
struct thread;
void QueueForSleep(struct thread* thr);
bool TryDequeueForSleep(struct thread* thr);