
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

void SecondProcessThread(void* arg) {
    int status = (int) (size_t) arg;
    SleepMilli(1000);
    KillProcess(status);
}

bool ok = false;

static void InitialProcessThread1(void*) {
    struct process* child1 = CreateProcessWithEntryPoint(1, SecondProcessThread, (void*) (size_t) 111);
    SleepMilli(300);
    struct process* child2 = CreateProcessWithEntryPoint(1, SecondProcessThread, (void*) (size_t) 222);
    SleepMilli(300);
    struct process* child3 = CreateProcessWithEntryPoint(1, SecondProcessThread, (void*) (size_t) 333);
    int retv;
    WaitProcess(GetPid(child3), &retv, 0);
    assert(retv == 333);
    WaitProcess(GetPid(child2), &retv, 0);
    assert(retv == 222);
    WaitProcess(GetPid(child1), &retv, 0);
    assert(retv == 111);
    ok = true;
}

static void InitialProcessThread2(void*) {
    pid_t c1pid = GetPid(CreateProcessWithEntryPoint(1, SecondProcessThread, (void*) (size_t) 111));
    SleepMilli(500);
    pid_t c2pid = GetPid(CreateProcessWithEntryPoint(1, SecondProcessThread, (void*) (size_t) 222));
    SleepMilli(500);
    pid_t c3pid = GetPid(CreateProcessWithEntryPoint(1, SecondProcessThread, (void*) (size_t) 333));
    
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

TFW_CREATE_TEST(BasicWaitTest) { TFW_IGNORE_UNUSED
    EXACT_IRQL(IRQL_STANDARD);
    CreateProcessWithEntryPoint(0, InitialProcessThread1, NULL);
    SleepMilli(2000);
    assert(ok);
}

TFW_CREATE_TEST(WaitTestWithNeg1) { TFW_IGNORE_UNUSED
    EXACT_IRQL(IRQL_STANDARD);
    CreateProcessWithEntryPoint(0, InitialProcessThread2, NULL);
    SleepMilli(3000);
    assert(ok);
}

void RegisterTfwWaitTests(void) {
    RegisterTfwTest("WaitProcess works (explict ids)", TFW_SP_ALL_CLEAR, BasicWaitTest, PANIC_UNIT_TEST_OK, 0);
    RegisterTfwTest("WaitProcess works (-1)", TFW_SP_ALL_CLEAR, WaitTestWithNeg1, PANIC_UNIT_TEST_OK, 0);
    // todo: waiting on someone that terminated long ago
    // todo: waiting on anything that terminated long ago
    // todo: stress tests
}

#endif
