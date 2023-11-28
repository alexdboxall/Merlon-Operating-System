#pragma once

#define THREAD_STATE_RUNNING    0
#define THREAD_STATE_READY      1
#define THREAD_STATE_BLOCKED    2

#define THREAD_PRIORITY_HIGH    0
#define THREAD_PRIORITY_NORMAL  127
#define THREAD_PRIORITY_LOW     254
#define THREAD_PRIORITY_IDLE    255

struct thread {
    struct vas* vas;
    size_t stack_pointer;
    size_t kernel_stack_top;
    size_t kernel_stack_size;
    void (*initial_address)(void*);

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
