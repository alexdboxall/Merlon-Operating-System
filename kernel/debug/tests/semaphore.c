
#include <semaphore.h>
#include <debug.h>
#include <assert.h>
#include <panic.h>
#include <string.h>
#include <log.h>
#include <arch.h>
#include <irql.h>
#include <thread.h>
#include <errno.h>
#include <virtual.h>

#ifndef NDEBUG

static bool Thread1Ok = false;
static void Thread1(void* sem_) {
    struct semaphore* sem = (struct semaphore*) sem_;

    int res = AcquireSemaphore(sem, 1500);
    Thread1Ok = true;
    assert(res == ETIMEDOUT);

    while (true) {
        Schedule();
    }
}

TFW_CREATE_TEST(SemaphoreTimeout1) { TFW_IGNORE_UNUSED
    EXACT_IRQL(IRQL_STANDARD);
    
    struct semaphore* sem = CreateSemaphore(1);   
    AcquireSemaphore(sem, -1);

    CreateThread(Thread1, (void*) sem, GetVas(), "");
    
    SleepMilli(1000);
    assert(!Thread1Ok);
    SleepMilli(1000);
    assert(Thread1Ok);
}


static void Thread2(void* sem_) {
    struct semaphore* sem = (struct semaphore*) sem_;

    int res = AcquireSemaphore(sem, 15000);
    Thread1Ok = true;
    assert(res == 0);

    while (true) {
        Schedule();
    }
}

TFW_CREATE_TEST(SemaphoreTimeout2) { TFW_IGNORE_UNUSED
    EXACT_IRQL(IRQL_STANDARD);
    
    struct semaphore* sem = CreateSemaphore(1);   
    AcquireSemaphore(sem, -1);

    CreateThread(Thread2, (void*) sem, GetVas(), "");
    
    SleepMilli(1000);
    ReleaseSemaphore(sem);
    SleepMilli(500);
    assert(Thread1Ok);
}


void RegisterTfwSemaphoreTests(void) {
    RegisterTfwTest("Semaphores with timeouts can be woken via timeout", TFW_SP_ALL_CLEAR, SemaphoreTimeout1, PANIC_UNIT_TEST_OK, 0);
    RegisterTfwTest("Semaphores with timeouts can be woken via release", TFW_SP_ALL_CLEAR, SemaphoreTimeout2, PANIC_UNIT_TEST_OK, 0);
}

#endif
