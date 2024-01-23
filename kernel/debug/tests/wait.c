
#include <semaphore.h>
#include <debug.h>
#include <assert.h>
#include <panic.h>
#include <string.h>
#include <log.h>
#include <arch.h>
#include <irql.h>
#include <thread.h>
#include <process.h>
#include <errno.h>
#include <virtual.h>
#include <stdlib.h>

#ifndef NDEBUG

static void SecondProcessThread(void* arg) {
    int status = (int) (size_t) arg;
    SleepMilli(1000);
    KillProcess(status);
}

static void ZombieProcess(void*) {
    KillProcess(99);
}

static void ZombieProcessWithDelay(void* arg) {
    SleepMilli((size_t) arg);
    KillProcess(23);
}

static bool ok = false;

static void InitialProcessThread1(void*) {
    pid_t ppid = GetPid(GetProcess());
    struct process* child1 = CreateProcessWithEntryPoint(ppid, SecondProcessThread, (void*) (size_t) 111);
    SleepMilli(300);
    struct process* child2 = CreateProcessWithEntryPoint(ppid, SecondProcessThread, (void*) (size_t) 222);
    SleepMilli(300);
    struct process* child3 = CreateProcessWithEntryPoint(ppid, SecondProcessThread, (void*) (size_t) 333);
    int retv;
    LogWriteSerial("ABC1\n");
    WaitProcess(child3->pid, &retv, 0);
    LogWriteSerial("ABC2\n");
    assert(retv == 333);
    WaitProcess(child2->pid, &retv, 0);
    LogWriteSerial("ABC3\n");
    assert(retv == 222);
    WaitProcess(child1->pid, &retv, 0);
    LogWriteSerial("ABC4\n");
    assert(retv == 111);
    ok = true;
}

static void InitialProcessThread2(void*) {
    pid_t ppid = GetPid(GetProcess());
    pid_t c1pid = CreateProcessWithEntryPoint(ppid, SecondProcessThread, (void*) (size_t) 111)->pid;
    SleepMilli(500);
    pid_t c2pid = CreateProcessWithEntryPoint(ppid, SecondProcessThread, (void*) (size_t) 222)->pid;
    SleepMilli(500);
    pid_t c3pid = CreateProcessWithEntryPoint(ppid, SecondProcessThread, (void*) (size_t) 333)->pid;
    
    int retv;
    pid_t pid = WaitProcess(-1, &retv, 0);
    assert(retv == 111);
    assert(pid == c1pid);

    pid = WaitProcess(c3pid, &retv, 0);
    assert(retv == 333);
    assert(pid == c3pid);

    pid = WaitProcess(-1, &retv, 0);
    assert(retv == 222);
    assert(pid == c2pid);

    ok = true;
}

static void InitialProcessThread3(void*) {
    pid_t ppid = GetPid(GetProcess());
    pid_t zombie = CreateProcessWithEntryPoint(ppid, ZombieProcess, NULL)->pid;
    SleepMilli(500);
    
    int retv;
    pid_t pid = WaitProcess(-1, &retv, 0);
    assert(retv == 99);
    assert(pid == zombie);
    ok = true;
}

static void InitialProcessThread4(void*) {
    pid_t ppid = GetPid(GetProcess());
    pid_t zombie = CreateProcessWithEntryPoint(ppid, ZombieProcess, NULL)->pid;
    SleepMilli(500);
    
    int retv;
    pid_t pid = WaitProcess(zombie, &retv, 0);
    assert(retv == 99);
    assert(pid == zombie);
    ok = true;
}

static void InitialProcessThread5(void* mode_) {
    size_t mode = (size_t) mode_;

    pid_t ppid = GetPid(GetProcess());

    LogWriteSerial("HOO??\n");

    pid_t pids[30];
    for (int i = 0; i < 30; ++i) {
        LogWriteSerial("CREATING PROCESS %d...\n", i);
        pids[i] = CreateProcessWithEntryPoint(ppid, ZombieProcess, NULL)->pid;
    }

    LogWriteSerial("HUH??\n");
    
    int retv;
    if (mode == 0) {
        for (int i = 0; i < 30; ++i) {
            LogWriteSerial("doing wait A.%d...\n", i);
            WaitProcess(-1, &retv, 0);
            assert(retv == 99);
        }
    } else if (mode == 1) {
        for (int i = 0; i < 30; ++i) {
            LogWriteSerial("doing wait B.%d...\n", i);
            pid_t pid = WaitProcess(pids[i], &retv, 0);
            assert(retv == 99);
            assert(pid == pids[i]);
        }
    } else {
        for (int i = 0; i < 30; ++i) {
            LogWriteSerial("doing wait C.%d...\n", i);
            pid_t pid = WaitProcess(pids[29 - i], &retv, 0);
            assert(retv == 99);
            assert(pid == pids[29 - i]);
        }
    }
    
    ok = true;
}

static int done_well = 0;
static struct spinlock done_well_lock;
 
static void InitialProcessThread6(void* j_) {
    int j = (size_t) j_;
    pid_t ppid = GetPid(GetProcess());
    pid_t pids[20];
    for (int i = 0; i < 10 + j; ++i) {
        pids[i] = CreateProcessWithEntryPoint(ppid, ZombieProcessWithDelay, (void*) (size_t) (rand() % 850))->pid;
        if (i % 2 == 0) {
            SleepMilli(rand() % 850);
        }
    }
    
    LogWriteSerial("In InitialProcessThread6... j = %d\n", j);
    int retv;
    int num_explicit = 5 + j / 3;
    for (int i = 3; i < 3 + num_explicit; ++i) {
        LogWriteSerial("About to wait implicit %d...\n", i - 3);
        WaitProcess(pids[i], &retv, 0);
        assert(retv == 23);
    }
    for (int i = 0; i < 10 + j - num_explicit; ++i) {
        LogWriteSerial("About to wait explicit %d...\n", i);
        WaitProcess(-1, &retv, 0);
        assert(retv == 23);
    }
    LogWriteSerial("Finished InitialProcessThread6... j = %d\n", j);
    AcquireSpinlock(&done_well_lock);
    ++done_well;
    ReleaseSpinlock(&done_well_lock);
}

TFW_CREATE_TEST(BasicWaitTest) { TFW_IGNORE_UNUSED
    EXACT_IRQL(IRQL_STANDARD);
    CreateProcessWithEntryPoint(0, InitialProcessThread1, NULL);
    SleepMilli(4000);
    assert(ok);
}

TFW_CREATE_TEST(WaitTestWithNeg1) { TFW_IGNORE_UNUSED
    EXACT_IRQL(IRQL_STANDARD);
    CreateProcessWithEntryPoint(0, InitialProcessThread2, NULL);
    SleepMilli(4000);
    assert(ok);
}

TFW_CREATE_TEST(WaitOnZombieTest1) { TFW_IGNORE_UNUSED
    EXACT_IRQL(IRQL_STANDARD);
    CreateProcessWithEntryPoint(0, InitialProcessThread3, NULL);
    SleepMilli(2000);
    assert(ok);
}

TFW_CREATE_TEST(WaitOnZombieTest2) { TFW_IGNORE_UNUSED
    EXACT_IRQL(IRQL_STANDARD);
    CreateProcessWithEntryPoint(0, InitialProcessThread4, NULL);
    SleepMilli(2000);
    assert(ok);
}

TFW_CREATE_TEST(WaitOnManyTest1) { TFW_IGNORE_UNUSED
    EXACT_IRQL(IRQL_STANDARD);
    CreateProcessWithEntryPoint(0, InitialProcessThread5, (void*) (size_t) 0);
    SleepMilli(8000);
    assert(ok);
}

TFW_CREATE_TEST(WaitOnManyTest2) { TFW_IGNORE_UNUSED
    EXACT_IRQL(IRQL_STANDARD);
    CreateProcessWithEntryPoint(0, InitialProcessThread5, (void*) (size_t) 1);
    SleepMilli(8000);
    assert(ok);
}

TFW_CREATE_TEST(WaitOnManyTest3) { TFW_IGNORE_UNUSED
    EXACT_IRQL(IRQL_STANDARD);
    CreateProcessWithEntryPoint(0, InitialProcessThread5, (void*) (size_t) 2);
    SleepMilli(8000);
    assert(ok);
}

TFW_CREATE_TEST(WaitStress) { TFW_IGNORE_UNUSED
    EXACT_IRQL(IRQL_STANDARD);
    InitSpinlock(&done_well_lock, "dwl", IRQL_SCHEDULER);
    done_well = 0;
    int m = ((size_t) context) + 15;
    for (int i = 0; i < m; ++i) {
        CreateProcessWithEntryPoint(0, InitialProcessThread6, (void*) (size_t) (i % 10));
        if (i > 7) {
            SleepMilli(rand() % 150 + 50);
        }
    }
    SleepMilli(6000);
    assert(done_well == m);
}

void RegisterTfwWaitTests(void) {
    RegisterTfwTest("WaitProcess works (explict ids)", TFW_SP_ALL_CLEAR, BasicWaitTest, PANIC_UNIT_TEST_OK, 0);
    RegisterTfwTest("WaitProcess works (-1)", TFW_SP_ALL_CLEAR, WaitTestWithNeg1, PANIC_UNIT_TEST_OK, 0);
    RegisterTfwTest("WaitProcess allows waiting on zombie (general)", TFW_SP_ALL_CLEAR, WaitOnZombieTest1, PANIC_UNIT_TEST_OK, 0);
    RegisterTfwTest("WaitProcess allows waiting on zombie (explicit)", TFW_SP_ALL_CLEAR, WaitOnZombieTest2, PANIC_UNIT_TEST_OK, 0);
    RegisterTfwTest("WaitProcess works when waiting on many (general)", TFW_SP_ALL_CLEAR, WaitOnManyTest1, PANIC_UNIT_TEST_OK, 0);
    RegisterTfwTest("WaitProcess works when waiting on many (explicit, in order)", TFW_SP_ALL_CLEAR, WaitOnManyTest2, PANIC_UNIT_TEST_OK, 0);
    RegisterTfwTest("WaitProcess works when waiting on many (explicit, reversed)", TFW_SP_ALL_CLEAR, WaitOnManyTest3, PANIC_UNIT_TEST_OK, 0);
    RegisterTfwTest("WaitProcess stress test (1)", TFW_SP_ALL_CLEAR, WaitStress, PANIC_UNIT_TEST_OK, 0);
    RegisterTfwTest("WaitProcess stress test (2)", TFW_SP_ALL_CLEAR, WaitStress, PANIC_UNIT_TEST_OK, 33);
    // todo: stress tests
}

#endif
