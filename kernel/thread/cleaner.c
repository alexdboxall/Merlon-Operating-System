
#include <thread.h>
#include <virtual.h>
#include <threadlist.h>
#include <irql.h>
#include <heap.h>
#include <semaphore.h>
#include <log.h>
#include <panic.h>
#include <physical.h>
#include <assert.h>

/*
 * Stores all of the threads which have been put in the THREAD_STATE_TERMINATED, but whose data
 * has not yet been deleted. The cleaner thread will eventually threads when they arrive in this list.
 */
static struct thread_list terminated_list;

/*
 * Used to block the cleaning thread until there is a thread in the `terminated_list`. When a task is
 * added to the list, the semaphore is released, and the cleaner tries to acquire it in an endless loop,
 * cleaning up threads whenever it is able to acquire it. This semaphore is initialised to an arbitrary
 * high value so that the cleaner initally blocks.
 */
static struct semaphore* cleaner_semaphore;

/**
 * Completely deallocates all data related to a given thread. Should be called by the cleaner
 * thread, on a thread that has already been terminated.
 * 
 * @param thr The terminated thread to deallocate data from. 
 * 
 * @note EXACT_IRQL(IRQL_STANDARD)
 */
static void PAGEABLE_CODE_SECTION CleanerDestroyThread(struct thread* thr) {
    MAX_IRQL(IRQL_PAGE_FAULT);   

    /*
     * TODO: clean up user stacks, any other thread data, etc.
     */
    LogWriteSerial("killing a thread stack... @ 0x%X\n", thr->kernel_stack_top - thr->kernel_stack_size);
    UnmapVirt(thr->kernel_stack_top - thr->kernel_stack_size, thr->kernel_stack_size);
    FreeHeap(thr->name);
    FreeHeap(thr);
}

/**
 * A thread which deletes/frees any threads that have been marked as terminated. We are required to
 * do this in a seperate thread, as terminated threads cannot clean themselves up (as that would involve
 * deleting their own stack, which they cannot safely do themselves.
 * 
 * Blocks until a thread is terminated.
 * 
 * @note MAX_IRQL(IRQL_PAGE_FAULT)
 */
static void PAGEABLE_CODE_SECTION CleanerThread(void*) {
    MAX_IRQL(IRQL_PAGE_FAULT);

    while (true) {
        /*
         * Block until there is a thread that needs terminating.
         */
        AcquireSemaphore(cleaner_semaphore, -1);

        LockScheduler();
        struct thread* thr = terminated_list.head;
        assert(thr != NULL);
        ThreadListDeleteTop(&terminated_list);
        UnlockScheduler();

        CleanerDestroyThread(thr);
    }
}

/**
 * Adds the cleaner task to the thread list. This must be called before any calls to `TerminateThread`
 * are made, and so this function should be called before multitasking is started.
 * 
 * @note MAX_IRQL(IRQL_SCHEDULER)
 */
void InitCleaner(void) {
    MAX_IRQL(IRQL_SCHEDULER);

    ThreadListInit(&terminated_list, NEXT_INDEX_TERMINATED);

    /*
     * The actual number here is meaningless, we just need it so that we can release
     * semaphores every time we delete a task, and the cleaner will block until then.
     * This number should be big though, as it is techincally a limit on the maximum number
     * of terminated (but uncleaned) tasks.
     */
    cleaner_semaphore = CreateSemaphore("cleaner", 1 << 30, 1 << 30);
    CreateThread(CleanerThread, NULL, GetVas(), "cleaner");
}

/**
 * A helper thread that started whenever a thread is added to the `terminated_list`. This wakes
 * the cleaner so it can clean up the thread.
 * 
 * @note MAX_IRQL(IRQL_SCHEDULER)
 */
static void NotifyCleaner(void*) {
    MAX_IRQL(IRQL_SCHEDULER);
    ReleaseSemaphore(cleaner_semaphore);    
}

void TerminateThreadLockHeld(struct thread* thr) {
    assert(thr != NULL);
    EXACT_IRQL(IRQL_SCHEDULER);

    if (thr == GetThread()) {
        ThreadListInsert(&terminated_list, thr);

        BlockThread(THREAD_STATE_TERMINATED);
        DeferUntilIrql(IRQL_STANDARD, NotifyCleaner, NULL);
        
    } else {
        /**
         * We can't terminate it directly, as it may be on any queue somewhere else,and it'd be very messy to
         * write special cases for all of them to be terminated while in a queue. It's much easier to just signal
         * that it needs to be terminated if it is scheduled to run again.
         */
        thr->death_sentence = true;
    }
}

/**
 * Terminates a thread. If the thread to terminate is the calling thread, this function will not return.
 * This does not cause the thread to immediately be deallocated, as it cannot do that while it is still 
 * executing (as it would lose its stack). If the thread being terminated is not the current thread, then
 * that thread will be terminated the next time it is scheduled to run (but may continue executing or sitting
 * on a blocked list until then). This function must not be called until after `InitCleaner` has been called.
 * The scheduler lock should not be already held.
 * 
 * @param thr The thread to terminated.
 * @return This function does not return if thr == GetThread(), and returns void otherwise.
 * 
 * @note MAX_IRQL(IRQL_SCHEDULER)
 */
void TerminateThread(struct thread* thr) {
    MAX_IRQL(IRQL_SCHEDULER);

    LockScheduler();
    TerminateThreadLockHeld(thr);
    UnlockScheduler();

    if (thr == GetThread()) {
        Panic(PANIC_IMPOSSIBLE_RETURN);
    }
}