#include <physical.h>
#include <virtual.h>
#include <heap.h>
#include <cpu.h>
#include <log.h>
#include <debug.h>
#include <assert.h>
#include <timer.h>
#include <irql.h>
#include <thread.h>
#include <panic.h>
#include <stdlib.h>
#include <process.h>

extern void InitDbgScreen(void);

void MyTestThread(void* str) {
    int count = 0;
    while (1) {
        (void) str;//DbgScreenPrintf("%s", str);
        SleepMilli(500);
        ++count;
        if (count == 10) {
            TerminateThread(GetThread());
        }
    }
}

void TfwTestingThread(void*) {
    MarkTfwStartPoint(TFW_SP_ALL_CLEAR);
    while (true) {
        Schedule();
    }
}

void SecondProcessThread(void*) {
    DbgScreenPrintf("The second process is waiting 2 seconds...\n");
    SleepMilli(2000);
    DbgScreenPrintf("The second process is about to terminate with status 123!\n");
    KillProcess(123);
}

void InitialProcessThread(void*) {
    DbgScreenPrintf("This was called by something living within a process.\n");

    struct process* child = CreateProcessWithEntryPoint(1, SecondProcessThread);
    int retv;
    WaitProcess(GetPid(child), &retv, 0);
    DbgScreenPrintf("The child process returned with status: %d\n", retv);

    while (true) {
        Schedule();
    }
}

void InitThread(void*) {
    CreateProcessWithEntryPoint(0, InitialProcessThread);

    while (true) {
        Schedule();
    }
}

void KernelMain(void) {
    LogWriteSerial("KernelMain: kernel is initialising...\n");

    /*
     * Allows us to call GetCpu(), which allows IRQL code to work. Anything which uses
     * IRQL (i.e. the whole system) relies on this, so this must be done first.
     */
    InitCpuTable();
    assert(GetIrql() == IRQL_STANDARD);

    /*
     * Initialise the testing framework if we're in debug mode.
     */
    InitTfw();
    MarkTfwStartPoint(TFW_SP_INITIAL);

    InitPhys();
    MarkTfwStartPoint(TFW_SP_AFTER_PHYS);

    InitHeap();

    /*
     * Allows deferments of functions to actually happen. IRQL is still usable beforehand though.
     */
    InitIrql();

    InitTimer();
    InitScheduler();
    MarkTfwStartPoint(TFW_SP_AFTER_HEAP);

    InitBootstrapCpu();
    MarkTfwStartPoint(TFW_SP_AFTER_BOOTSTRAP_CPU);

    InitVirt();
    MarkTfwStartPoint(TFW_SP_AFTER_VIRT);

    ReinitPhys();
    MarkTfwStartPoint(TFW_SP_AFTER_PHYS_REINIT);

    InitOtherCpu();
    MarkTfwStartPoint(TFW_SP_AFTER_ALL_CPU);

    InitProcess();

    InitDbgScreen();
    DbgScreenPrintf("\n  NOS Kernel\n  Copyright Alex Boxall 2022-2023\n\n  %d / %d KB used\n\n  ...", GetTotalPhysKilobytes() - GetFreePhysKilobytes(), GetTotalPhysKilobytes());

    CreateThread(MyTestThread, "1", GetVas(), "test thread!");
    CreateThread(MyTestThread, "2", GetVas(), "test thread!");
    CreateThread(MyTestThread, "3", GetVas(), "test thread!");
    CreateThread(MyTestThread, "4", GetVas(), "test thread!");
    CreateThread(MyTestThread, "5", GetVas(), "test thread!");

    CreateThread(TfwTestingThread, NULL, GetVas(), "twf all clear tests");
    CreateThread(InitThread, NULL, GetVas(), "init1");
    StartMultitasking();
}