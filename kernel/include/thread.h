#pragma once

#include <common.h>
#include <signal.h>

struct semaphore;
struct process;

#define THREAD_STATE_RUNNING                                0
#define THREAD_STATE_READY                                  1
#define THREAD_STATE_SLEEPING                               2
#define THREAD_STATE_WAITING_FOR_SEMAPHORE                  3
#define THREAD_STATE_WAITING_FOR_SEMAPHORE_WITH_TIMEOUT     4
#define THREAD_STATE_TERMINATED                             5
#define THREAD_STATE_STOPPED                                6
#define THREAD_STATE_WAITING_FOR_SIGNAL                     7

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
#define NEXT_INDEX_TERMINATED  0        // terminated can share the ready list

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
    bool timed_out_due_to_signal;
    bool needs_termination;
    bool needs_stopping;        // SIGSTOP

    struct semaphore* waiting_on_semaphore;

    struct process* process;

    bool signal_intr;
    sigset_t pending_signals;
    sigset_t blocked_signals;
    sigset_t prev_blocked_signals;
    size_t user_common_signal_handler;

    /*
     * The system time at which this task's time has expired. If this is 0, then the task will not have a set time limit.
     * This value is set to GetSystemTimer() + TIMESLICE_LENGTH_MS when the task is scheduled in, and doesn't change until
     * the next time it is switched in.
     */
    uint64_t timeslice_expiry;
    uint64_t gifted_timeslice;

    uint64_t sleep_expiry;
    int alarm_id;
};

bool HasBeenSignalled();

void Schedule(void);
void LockScheduler(void);
void UnlockScheduler(void);
void PreventScheduler(void);
void UnpreventScheduler(void);

void InitScheduler(void);
void StartMultitasking(void);

void AssertSchedulerLockHeld(void);

#include <cpu.h>
#define GetThread(...) (GetCpu()->current_thread)

void TerminateThread(struct thread* thr);
void TerminateThreadLockHeld(struct thread* thr);

void CreateInitialForkThread(struct process* prcss, struct thread* old);
struct thread* CreateThreadEx(void(*entry_point)(void*), void* argument, struct vas* vas, const char* name, struct process* prcss, int policy, int priority, int kernel_stack_kb);
struct thread* CreateThread(void(*entry_point)(void*), void* argument, struct vas* vas, const char* name);

void BlockThread(int reason);
void UnblockThread(struct thread* thr);
void UnblockThreadGiftingTimeslice(struct thread* thr);
int SetThreadPriority(struct thread* thread, int policy, int priority);

void StopThread(struct thread* thr);
void ContinueThread(struct thread* thr);

int SleepUntil(uint64_t system_time_ns);
int SleepNano(uint64_t delta_ns);
int SleepMilli(uint32_t delta_ms);

void HandleSleepWakeups(void* sys_time_ptr); // used internally between timer.c and thread.c
void InitIdle(void);
void InitCleaner(void);

struct process* CreateUsermodeProcess(struct process* parent, const char* filename);

/*
 * A thread can lock itself onto the current cpu. Task switches *STILL OCCUR*, but we ensure that
 * next time this task runs, it will go back to this cpu.
 * 
 * This is not a spinlock nor mutex, it's literally should just set a flag in the thread struct (sure, that
 * will spin while setting variable, but that's it). Between AssignThreadToCpu and UnassignThreadToCpu we remain
 * at IRQL_STANDARD.
 */
void AssignThreadToCpu(void);
void UnassignThreadToCpu(void);

