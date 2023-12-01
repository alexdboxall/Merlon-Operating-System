
#include <thread.h>
#include <virtual.h>
#include <threadlist.h>
#include <irql.h>
#include <semaphore.h>
#include <log.h>
#include <panic.h>
#include <assert.h>

static struct thread_list terminated_list;
static struct semaphore* cleaner_semaphore;

static void CleanerDestroyThread(struct thread* thr) {
    (void) thr;
    LogWriteSerial("TODO: clean up the thread!\n");
}

static void CleanerThread(void* ignored) {
    (void) ignored;

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

void InitCleaner(void) {
    ThreadListInit(&terminated_list, NEXT_INDEX_TERMINATED);

    /*
     * The actual number here is meaningless, we just need it so that we can release
     * semaphores every time we delete a task, and the cleaner will block until then.
     * This number should be big though, as it is techincally a limit on the maximum number
     * of terminated (but uncleaned) tasks.
     */
    cleaner_semaphore = CreateSemaphore(1 << 30, 1 << 30);

    CreateThread(CleanerThread, NULL, GetVas(), "cleaner");
}

static void NotifyCleaner(void* ignored) {
    (void) ignored;

    /*
     * Unblock the cleaner so it can delete our task.
     */
    ReleaseSemaphore(cleaner_semaphore);    
}

void TerminateThread(void) {
    LockScheduler();
    ThreadListInsert(&terminated_list, GetThread());
    BlockThread(THREAD_STATE_TERMINATED);
    DeferUntilIrql(IRQL_STANDARD, NotifyCleaner, NULL);
    UnlockScheduler();
    Panic(PANIC_IMPOSSIBLE_RETURN);
}