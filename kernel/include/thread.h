#pragma once

#include <common.h>

struct semaphore;

#define THREAD_STATE_RUNNING                                0
#define THREAD_STATE_READY                                  1
#define THREAD_STATE_SLEEPING                               2
#define THREAD_STATE_WAITING_FOR_SEMAPHORE                  3
#define THREAD_STATE_WAITING_FOR_SEMAPHORE_WITH_TIMEOUT     4

#define SCHEDULE_POLICY_FIXED             0
#define SCHEDULE_POLICY_USER_HIGHER       1
#define SCHEDULE_POLICY_USER_NORMAL       2
#define SCHEDULE_POLICY_USER_LOWER        3

#define FIXED_PRIORITY_KERNEL_HIGH        0
#define FIXED_PRIORITY_KERNEL_NORMAL      30
#define FIXED_PRIORITY_IDLE               255

/*
 * Determines which of the 'next' pointers are used to manage the list.
 * A thread can be on multiple lists so long as they are different numbers.
 * Can increase the number of 'next' pointers in the thread struct to make them distinct if needed.
 */
#define NEXT_INDEX_READY       0
#define NEXT_INDEX_SLEEP       1
#define NEXT_INDEX_SEMAPHORE   2

struct thread {
    /*
     * These first two values must be in this order.
     */
    size_t kernel_stack_top;
    size_t stack_pointer;

    struct vas* vas;
    size_t kernel_stack_size;
    void (*initial_address)(void*);

    /*
     * Allows a thread to be on a timer and a semaphore list at the same time.
     * Very sketchy stuff.
     */
    struct thread* next[3];

    int thread_id;
    int state;
    void* argument;
    uint64_t time_used;
    char* name;
    int priority;
    int schedule_policy;
    size_t canary_position;
    bool timed_out;
    
    struct semaphore* waiting_on_semaphore;
    
    /*
     * The system time at which this task's time has expired. If this is 0, then the task will not have a set time limit.
     * This value is set to GetSystemTimer() + TIMESLICE_LENGTH_MS when the task is scheduled in, and doesn't change until
     * the next time it is switched in.
     */
    uint64_t timeslice_expiry;

    uint64_t sleep_expiry;
};

void Schedule(void);
void LockScheduler(void);
void UnlockScheduler(void);

void InitScheduler(void);
void StartMultitasking(void);

void AssertSchedulerLockHeld(void);

struct thread* GetThread(void);
void TerminateThread(void);
struct thread* CreateThread(void(*entry_point)(void*), void* argument, struct vas* vas, const char* name);
void BlockThread(int reason);
void UnblockThread(struct thread* thr);
int SetThreadPriority(struct thread* thread, int policy, int priority);

void SleepUntil(uint64_t system_time_ns);
void SleepNano(uint64_t delta_ns);
void SleepMilli(uint32_t delta_ms);

void HandleSleepWakeups(void* sys_time_ptr); // used internally between timer.c and thread.c
void InitIdle(void);