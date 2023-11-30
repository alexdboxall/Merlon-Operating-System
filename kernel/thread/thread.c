
#include <cpu.h>
#include <thread.h>
#include <spinlock.h>
#include <heap.h>
#include <assert.h>
#include <timer.h>
#include <string.h>
#include <irql.h>
#include <log.h>
#include <errno.h>
#include <virtual.h>
#include <panic.h>
#include <common.h>
#include <threadlist.h>
#include <priorityqueue.h>

static struct thread_list ready_list;
static struct spinlock scheduler_lock;
static struct spinlock innermost_lock;

/*
 * Because these IRQL jumps need to persist across switches, we can't just chuck
 * it on the stack like normal. No need for nesting / a stack to store these, as the
 * scheduler lock cannot be nested.
 */
static int scheduler_lock_irql;

/*
* Local fixed sized arrays and variables need to fit on the kernel stack.
* Allocate at least 8KB (depending on the system page size).
*
* Please note that overflowing the kernel stack into non-paged memory will lead to
* an immediate and unrecoverable crash on most systems.
*/
#define KERNEL_STACK_SIZE   BytesToPages(1024 * 4) * ARCH_PAGE_SIZE

/*
* The user stack is allocated as needed - this is the maximum size of the stack in
* user virtual memory. (However, a larger max stack means more page tables need to be
* allocated to store it - even if there are no actual stack pages in yet).
*
* On x86, allocating a 4MB region only requires one page table, hence we'll use that.
*/
#define USER_STACK_MAX_SIZE BytesToPages(1024 * 1024 * 4) * ARCH_PAGE_SIZE


#define TIMESLICE_LENGTH_MS 50

/*
* Kernel stack overflow normally results in a total system crash/reboot because 
* fault handlers will not work (they push data to a non-existent stack!).
*
* We will fill pages at the end of the stack with a certain value (CANARY_VALUE),
* and then we can check if they have been modified. If they are, we will throw a 
* somewhat nicer error than a system reboot.
*
* Note that we can still overflow 'badly' if someone makes an allocation on the
* stack lwhich is larger than the remaining space on the stack and the canary size
* combined.
*
* If the canary page is only partially used for the canary, the remainder of the
* page is able to be used normally.
*/
#ifdef NDEBUG
#define NUM_CANARY_PAGES 0
#else
#define NUM_CANARY_BYTES (1024 * 8)
#define NUM_CANARY_PAGES BytesToPages(NUM_CANARY_BYTES)
#define CANARY_VALUE     0x8BADF00D

static void CheckCanary(size_t canary_base) {
    uint32_t* canary_ptr = (uint32_t*) canary_base;

    for (size_t i = 0; i < NUM_CANARY_BYTES / sizeof(uint32_t); ++i) {
        if (*canary_ptr++ != CANARY_VALUE) {
            Panic(PANIC_CANARY_DIED);
        }
    }
}

static void CreateCanary(size_t canary_base) {
    uint32_t* canary_ptr = (uint32_t*) canary_base;

    for (size_t i = 0; i < NUM_CANARY_BYTES / sizeof(uint32_t); ++i) {
        *canary_ptr++ = CANARY_VALUE;
    }
}

#endif


/*
* Allocates a new page-aligned stack for a kernel thread, and returns
* the address of either the top of the stack (if it grows downward),
* or the bottom (if it grows upward).
*/
static void CreateKernelStacks(struct thread* thr) {
    int total_bytes = (BytesToPages(KERNEL_STACK_SIZE) + NUM_CANARY_PAGES) * ARCH_PAGE_SIZE;
    
    size_t stack_bottom = MapVirt(0, 0, total_bytes, VM_READ | VM_WRITE | VM_LOCK, NULL, 0);
    size_t stack_top = stack_bottom + total_bytes;

#ifndef NDEBUG
    thr->canary_position = stack_bottom;
    CreateCanary(stack_bottom);
#endif
    thr->kernel_stack_top = stack_top;
    thr->kernel_stack_size = total_bytes;

    thr->stack_pointer = ArchPrepareStack(thr->kernel_stack_top);
}

/*static*/ size_t CreateUserStack(int size) {
    /*
    * All user stacks share the same area of virtual memory, but have different
    * mappings to physical memory.
    */

    int total_bytes = BytesToPages(size) * ARCH_PAGE_SIZE;
    size_t stack_base = ARCH_USER_STACK_LIMIT - total_bytes;
    size_t actual_base = MapVirt(0, stack_base, total_bytes, VM_READ | VM_WRITE | VM_USER, NULL, 0);
    
    assert(stack_base == actual_base);
    (void) actual_base;     // the assert gets taken out on release mode, so make the compiler happy
    
    return ARCH_USER_STACK_LIMIT;
}

/**
 * Blocks the currently executing thread (no effect will happen until the IRQL goes below IRQL_SCHEDULER).
 * The scheduler lock must be held. 
 */

void BlockThread(int reason) {
    AssertSchedulerLockHeld();
    assert(reason != THREAD_STATE_READY && reason != THREAD_STATE_RUNNING);
    assert(GetCpu()->current_thread->state == THREAD_STATE_RUNNING);
    GetCpu()->current_thread->state = reason;
    PostponeScheduleUntilStandardIrql();
}

void UnblockThread(struct thread* thr) { 
    AssertSchedulerLockHeld();
    ThreadListInsert(&ready_list, thr);
}

void TerminateThread(void) {

}

void UpdateThreadTimeUsed(void) {
    static uint64_t prev_time = 0;

    uint64_t time = GetSystemTimer();
    uint64_t time_elapsed = GetSystemTimer() - prev_time;
    prev_time = time;

    GetThread()->time_used += time_elapsed;
}

static int GetNextThreadId(void) {
    static struct spinlock thread_id_lock;
    static int next_thread_id = 0;
    static bool initialised = false;

    if (!initialised) {
        initialised = true;
        InitSpinlock(&thread_id_lock, "thread id", IRQL_SCHEDULER);
    }

    int irql = AcquireSpinlock(&thread_id_lock, true);
    int result = next_thread_id++;
    ReleaseSpinlockAndLower(&thread_id_lock, irql);
    return result;
}

struct thread* CreateThread(void(*entry_point)(void*), void* argument, struct vas* vas, const char* name) {
    struct thread* thr = AllocHeap(sizeof(struct thread));
    thr->argument = argument;
    thr->initial_address = entry_point;
    thr->state = THREAD_STATE_READY;
    thr->time_used = 0;
    thr->name = strdup(name);
    thr->priority = FIXED_PRIORITY_KERNEL_NORMAL;
    thr->schedule_policy = SCHEDULE_POLICY_FIXED;
    thr->timeslice_expiry = GetSystemTimer() + TIMESLICE_LENGTH_MS;
    thr->vas = vas;
    thr->thread_id = GetNextThreadId();
    CreateKernelStacks(thr);

    LockScheduler();
    ThreadListInsert(&ready_list, thr);
    UnlockScheduler();

    return thr;
}

struct thread* GetThread(void) {
    return GetCpu()->current_thread;
}

static void UpdateTimesliceExpiry(void) {
    struct thread* thr = GetThread();
    thr->timeslice_expiry = GetSystemTimer() + (thr->priority == 255 ? 0 : (20 + thr->priority / 4) * 1000000ULL);
}

void ThreadInitialisationHandler(void) {
    /*
     * This normally happends in the schedule code, just after the call to ArchSwitchThread,
     * but we forced ourselves to jump here instead, so we'd better do it now.
     */
    ReleaseSpinlockAndLower(&innermost_lock, IRQL_SCHEDULER);
    UpdateTimesliceExpiry();

    /*
    * To get here, someone must have called thread_schedule(), and therefore
    * the lock must have been held.
    */
    UnlockScheduler();


    /* Anything else you might want to do should be done here... */

    /* Go to the address the thread actually wanted. */
    GetThread()->initial_address(GetThread()->argument);   

    /* The thread has returned, so just terminate it. */
    TerminateThread();
}

static int GetMinPriorityValueForPolicy(int policy) {
    if (policy == SCHEDULE_POLICY_USER_HIGHER) return 50;
    if (policy == SCHEDULE_POLICY_USER_NORMAL) return 100;
    if (policy == SCHEDULE_POLICY_USER_LOWER) return 150;
    return 0;
}

static int GetMaxPriorityValueForPolicy(int policy) {
    if (policy == SCHEDULE_POLICY_FIXED) return 255;
    return GetMinPriorityValueForPolicy(policy)  + 100;
}

static void UpdatePriority(bool yielded) {
    struct thread* thr = GetThread();
    int policy = thr->schedule_policy;

    if (policy != SCHEDULE_POLICY_FIXED) {
        int new_val = thr->priority + (yielded ? -1 : 1);
        if (new_val >= GetMinPriorityValueForPolicy(policy) && new_val <= GetMaxPriorityValueForPolicy(policy)) {
            thr->priority = new_val;
        }
    }
}

void LockScheduler(void) {
    scheduler_lock_irql = AcquireSpinlock(&scheduler_lock, true);
}

void UnlockScheduler(void) {
    ReleaseSpinlockAndLower(&scheduler_lock, scheduler_lock_irql);
}

void AssertSchedulerLockHeld(void) {
    assert(IsSpinlockHeld(&scheduler_lock));
}

static void SwitchToNewTask(struct thread* old_thread, struct thread* new_thread) {
    if (new_thread->vas != old_thread->vas) {
        SetVas(new_thread->vas);
    }

    new_thread->state = THREAD_STATE_RUNNING;
    ThreadListDeleteTop(&ready_list);

    /*
     * No IRQs allowed while this happens, as we need to protect the CPU structure.
     * Only our CPU has access to it (as it is per-CPU), but if we IRQ and then someone
     * calls GetCpu(), we'll be in a bit of strife.
     */
    struct cpu* cpu = GetCpu();
    AcquireSpinlock(&innermost_lock, true);
    cpu->current_thread = new_thread;
    cpu->current_vas = new_thread->vas;
    ArchSwitchThread(old_thread, new_thread);

    /*
     * This code doesn't get called on the first time a thread gets run!! It jumps straight from
     * ArchSwitchThread to ThreadInitialisationHandler!
     */
    ReleaseSpinlockAndLower(&innermost_lock, IRQL_SCHEDULER);
    UpdateTimesliceExpiry();
}

static void ScheduleWithLockHeld(void) {
    EXACT_IRQL(IRQL_SCHEDULER);
    AssertSchedulerLockHeld();

    struct thread* old_thread = GetThread();
    struct thread* new_thread = ready_list.head;

    if (old_thread == NULL) {
        /*
         * Multitasking not set up yet. Now check if someone has added a task that we can switch to.
         * (If not, we keep waiting until they have, then we can start multitasking).
         */
        if (ready_list.head != NULL) {
            /*
             * We need a place where it can write the "old" stack pointer to.
             */
            struct thread dummy;
            SwitchToNewTask(&dummy, new_thread);
        }
        return;
    }
    
    if (new_thread == old_thread || new_thread == NULL) {
        /*
         * Don't switch if there isn't anything else to switch to!
         */
        return;
    }

#ifndef NDEBUG
    CheckCanary(old_thread->canary_position);
#endif

    bool yielded = old_thread->timeslice_expiry > GetSystemTimer();
    UpdatePriority(yielded);
    UpdateThreadTimeUsed();

    /*
     * Put the old task back on the ready list, but only if it didn't block / get suspended.
     */
    if (old_thread->state == THREAD_STATE_RUNNING) {
        ThreadListInsert(&ready_list, old_thread);
    }
    SwitchToNewTask(old_thread, new_thread);
}

void Schedule(void) {
    if (GetIrql() != IRQL_STANDARD) {
        PostponeScheduleUntilStandardIrql();
        return;
    }

    LockScheduler();
    ScheduleWithLockHeld();
    UnlockScheduler();
}


void InitScheduler() {
    ThreadListInit(&ready_list);
    InitSpinlock(&scheduler_lock, "scheduler", IRQL_SCHEDULER);
    InitSpinlock(&innermost_lock, "inner scheduler", IRQL_HIGH);
}

[[noreturn]] void StartMultitasking(void) {
    InitIdle();

    /*
     * Once this is called, "the game is afoot!" and threads will start running.
     */
    Schedule();
    
    while (1) {
        ;
    }
}


/**
 * Sets the priority and/or policy of a thread.
 * 
 * @param policy    The scheduling policy to use. Should be one of SCHEDULE_POLICY_FIXED, SCHEDULE_POLICY_USER_LOWER,
 *                      SCHEDULE_POLICY_USER_NORMAL, SCHEDULE_POLICY_USER_HIGHER, or -1. Set to -1 to indicate that the 
 *                      policy should not be changed.
 * @param priority  The priority level to give the thread. Should be a value between 0 and 255, where 0 indicates the highest
 *                      possible priority. A value of 255 indicates that the thread should only be run when the system is dile.
 *                      If the thread's policy is SCHEDULE_POLICY_FIXED, then this value will get used directly. For other
 *                      policies, the actual priority given to the thread may differ depending on the rules of the policy. 
 *                      Set to -1 to indicate that the policy should not be changed.
 * 
 * @return Returns 0 on success, EINVAL if invalid arguments are given. If EINVAL is returned, no change will be made to the thread's
 *         policy or priority.
 * 
 * @user This function may be used as a system call, as long as 'thr' points to a valid thread structure (which it should do,
 *       as the user will probably supply thread number, which the kernel then converts to address - or kernel may just make it
 *       a 'current thread' syscall, in which case GetThread() will be valid.
 * 
 * @maxirql IRQL_HIGH
 */
int SetThreadPriority(struct thread* thr, int policy, int priority) { 
    if (priority < -1 || priority > 255) {
        return EINVAL;
    }
    if (policy != -1 && policy != SCHEDULE_POLICY_FIXED && policy != SCHEDULE_POLICY_USER_LOWER && policy != SCHEDULE_POLICY_USER_NORMAL && policy != SCHEDULE_POLICY_USER_HIGHER) {
        return EINVAL;
    }

    if (priority < GetMinPriorityValueForPolicy(policy)) {
        priority = GetMinPriorityValueForPolicy(policy);
    }
    if (priority > GetMaxPriorityValueForPolicy(policy)) {
        priority = GetMaxPriorityValueForPolicy(policy);
    }

    if (policy != -1) {
        thr->schedule_policy = policy;
    }
    if (priority != -1) {
        thr->priority = priority;
    }

    return 0;
}