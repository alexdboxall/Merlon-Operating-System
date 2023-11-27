
#include <cpu.h>
#include <thread.h>
#include <spinlock.h>
#include <schedule.h>
#include <heap.h>
#include <assert.h>
#include <timer.h>
#include <string.h>
#include <irql.h>
#include <virtual.h>
#include <panic.h>

/*
* Local fixed sized arrays and variables need to fit on the kernel stack.
* Allocate at least 8KB (depending on the system page size).
*
* Please note that overflowing the kernel stack into non-paged memory will lead to
* an immediate and unrecoverable crash on most systems.
*/
#define KERNEL_STACK_SIZE   BytesToPages(1024 * 8) * ARCH_PAGE_SIZE

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
#define NUM_CANARY_BYTES (1024 * 8)
#else
#define NUM_CANARY_BYTES 2048
#endif
#define NUM_CANARY_PAGES 1

#define CANARY_VALUE     0x8BADF00D

static void CreateCanary(size_t canary_base) {
    uint32_t* canary_ptr = (uint32_t*) canary_base;

    for (size_t i = 0; i < NUM_CANARY_BYTES / sizeof(uint32_t); ++i) {
        *canary_ptr++ = CANARY_VALUE;
    }
}

void CheckCanary(size_t canary_base) {
    uint32_t* canary_ptr = (uint32_t*) canary_base;

    for (size_t i = 0; i < NUM_CANARY_BYTES / sizeof(uint32_t); ++i) {
        if (*canary_ptr++ != CANARY_VALUE) {
            Panic(PANIC_CANARY_DIED);
        }
    }
}

/*
* Allocates a new page-aligned stack for a kernel thread, and returns
* the address of either the top of the stack (if it grows downward),
* or the bottom (if it grows upward).
*/
static void CreateKernelStack(struct thread* thr) {
    int total_bytes = (BytesToPages(KERNEL_STACK_SIZE) + NUM_CANARY_PAGES) * ARCH_PAGE_SIZE;
    
    size_t stack_bottom = MapVirt(0, 0, total_bytes, VM_READ | VM_WRITE | VM_LOCK, NULL, 0);
    size_t stack_top = stack_bottom + total_bytes;

    thr->canary_position = stack_bottom;
    CreateCanary(stack_bottom);
    
    thr->kernel_stack_top = stack_top;
    thr->kernel_stack_size = total_bytes;

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
    ScheduleWithLockHeld();
}

void UnblockThread(void) { 

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
    thr->priority = THREAD_PRIORITY_NORMAL;
    thr->timeslice_expiry = GetSystemTimer() + TIMESLICE_LENGTH_MS;
    thr->vas = vas;
    thr->thread_id = GetNextThreadId();

    CreateKernelStack(thr);

    // thr->stack_pointer = ArchPrepareStack(thr);      // arch_prepare_stack(thr->kernel_stack_top);

    // TODO: add to the ready list

    return thr;
}

struct thread* GetThread(void) {
    return GetCpu()->current_thread;
}

void ThreadInitialisationHandler(void) {
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