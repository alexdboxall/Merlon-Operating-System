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

extern void InitDbgScreen(void);

void MyTestThread(void* str) {
    while (1) {
        DbgScreenPrintf("%s", str);
        SleepMilli(500);
    }
}

void TfwTestingThread(void* ignored) {
    (void) ignored;

    MarkTfwStartPoint(TFW_SP_ALL_CLEAR);
    while (true) {
        Schedule();
    }
}

void KernelMain(void) {
    LogWriteSerial("KernelMain: kernel is initialising...\n");

    InitTfw();
    MarkTfwStartPoint(TFW_SP_INITIAL);

    InitPhys();
    MarkTfwStartPoint(TFW_SP_AFTER_PHYS);

    InitHeap();
    InitIrql();
    InitTimer();
    InitScheduler();
    assert(GetIrql() == IRQL_STANDARD);
    MarkTfwStartPoint(TFW_SP_AFTER_HEAP);

    InitBootstrapCpu();
    MarkTfwStartPoint(TFW_SP_AFTER_BOOTSTRAP_CPU);

    InitVirt();
    MarkTfwStartPoint(TFW_SP_AFTER_VIRT);

    ReinitPhys();
    MarkTfwStartPoint(TFW_SP_AFTER_PHYS_REINIT);

    InitOtherCpu();
    MarkTfwStartPoint(TFW_SP_AFTER_ALL_CPU);

    InitDbgScreen();
    DbgScreenPrintf("\n  NOS Kernel\n  Copyright Alex Boxall 2022-2023\n\n  %d / %d KB used\n\n  ...", GetTotalPhysKilobytes() - GetFreePhysKilobytes(), GetTotalPhysKilobytes());

    CreateThread(MyTestThread, "1", GetVas(), "test thread!");
    CreateThread(MyTestThread, "2", GetVas(), "test thread!");
    CreateThread(MyTestThread, "3", GetVas(), "test thread!");
    CreateThread(MyTestThread, "4", GetVas(), "test thread!");
    CreateThread(MyTestThread, "5", GetVas(), "test thread!");

    CreateThread(TfwTestingThread, NULL, GetVas(), "twf all clear tests");

    StartMultitasking();
}