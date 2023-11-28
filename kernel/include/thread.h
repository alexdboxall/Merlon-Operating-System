#pragma once

#define THREAD_STATE_RUNNING    0
#define THREAD_STATE_READY      1
#define THREAD_STATE_BLOCKED    2

struct thread {
    /*
     * These first two values must be in this order.
     */
    size_t kernel_stack_top;
    size_t stack_pointer;

    struct vas* vas;
    size_t kernel_stack_size;
    void (*initial_address)(void*);
    struct thread* next;

    int thread_id;
    int state;
    void* argument;
    uint64_t time_used;
    char* name;
    int priority;
    int schedule_policy;
    size_t canary_position;
    
    /*
     * The system time at which this task's time has expired. If this is 0, then the task will not have a set time limit.
     * This value is set to GetSystemTimer() + TIMESLICE_LENGTH_MS when the task is scheduled in, and doesn't change until
     * the next time it is switched in.
     */
    uint64_t timeslice_expiry;
};

struct thread* GetThread(void);
void BlockThread(int reason);
void UnblockThread(void);
void TerminateThread(void);
struct thread* CreateThread(void(*entry_point)(void*), void* argument, struct vas* vas, const char* name);