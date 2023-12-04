
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
#include <stdlib.h>

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
    
    struct semaphore* sem = CreateSemaphore(1, 0);
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
    
    struct semaphore* sem = CreateSemaphore(1, 0);  
    AcquireSemaphore(sem, -1);

    CreateThread(Thread2, (void*) sem, GetVas(), "");
    
    SleepMilli(1000);
    ReleaseSemaphore(sem);
    SleepMilli(500);
    assert(Thread1Ok);
}

static void Thread3(void* ignored) {
    (void) ignored;

    while (true) {
        if (rand() % 10 == 0) {
            for (int j = 0; j < rand() % 1000000; ++j) {
                DbgScreenPrintf("z");
            }
        }
        SleepMilli(rand() % 150);
    }
}

static void Thread3B(void* ignored) {
    (void) ignored;

    while (true) {
        SleepMilli(rand() % 100);
    }
}

static void Thread4(void* ignored) {
    (void) ignored;

    while (true) {
        DbgScreenPrintf("y");
    }
}

static struct semaphore* sems[20];

void Thread5(void* delay) {
    int loops = 0;

    while (true) {
        int a = rand() % 20;
        int b = rand() % 8;
        int c = rand() % 3;
        if (a == b || a == c || b == c) {
            continue;
        }

        int ares = AcquireSemaphore(sems[a], (rand() % 10) * (rand() % 10));
        int bres = AcquireSemaphore(sems[b], (rand() % 10) * (rand() % 10) * 2);
        int cres = AcquireSemaphore(sems[c], (rand() % 10) * (rand() % 10) * 3);
        if (delay != NULL) {
            SleepMilli(5);
        }
        if (ares == 0) {
            ReleaseSemaphore(sems[a]);
        }
        if (cres == 0) {
            ReleaseSemaphore(sems[c]);
        }
        if (bres == 0) {
            ReleaseSemaphore(sems[b]);
        }
        ++loops;
        if (rand() % 3 == 0) {
            DbgScreenPrintf("%d,", loops);
        }
    }
}

TFW_CREATE_TEST(SchedulerHeartAttack) { TFW_IGNORE_UNUSED
    EXACT_IRQL(IRQL_STANDARD);

    for (int i = 0; i < 20; ++i) {
        sems[i] = CreateSemaphore(rand() % 3 + 1, 0);
    }

    for (int i = 0; i < 10; ++i) {
        CreateThread(Thread3, NULL, GetVas(), "");
    }
    for (int i = 0; i < 10; ++i) {
        CreateThread(Thread3B, NULL, GetVas(), "");
    }
    for (int i = 0; i < 5; ++i) {
        CreateThread(Thread4, NULL, GetVas(), "");
    }
    for (int i = 0; i < 100; ++i) {
        CreateThread(Thread5, context % 2 == 0 ? NULL : ((void*) 1), GetVas(), "");
    }

    for (int i = 0; i < (int) context; ++i) {
        SleepMilli(1000);
    }
}

void RegisterTfwSemaphoreTests(void) {
    RegisterTfwTest("Semaphores with timeouts can be woken via timeout", TFW_SP_ALL_CLEAR, SemaphoreTimeout1, PANIC_UNIT_TEST_OK, 0);
    RegisterTfwTest("Semaphores with timeouts can be woken via release", TFW_SP_ALL_CLEAR, SemaphoreTimeout2, PANIC_UNIT_TEST_OK, 0);
    RegisterTfwTest("Scheduler stress test with semaphores (1)", TFW_SP_ALL_CLEAR, SchedulerHeartAttack, PANIC_UNIT_TEST_OK, 15);
    RegisterTfwTest("Scheduler stress test with semaphores (2)", TFW_SP_ALL_CLEAR, SchedulerHeartAttack, PANIC_UNIT_TEST_OK, 20);
    RegisterNightlyTfwTest("Scheduler stress test with semaphores (3)", TFW_SP_ALL_CLEAR, SchedulerHeartAttack, PANIC_UNIT_TEST_OK, 80);
    RegisterNightlyTfwTest("Scheduler stress test with semaphores (4)", TFW_SP_ALL_CLEAR, SchedulerHeartAttack, PANIC_UNIT_TEST_OK, 125);
    RegisterNightlyTfwTest("Scheduler stress test with semaphores (5)", TFW_SP_ALL_CLEAR, SchedulerHeartAttack, PANIC_UNIT_TEST_OK, 600);
}

#endif
