#include <physical.h>
#include <virtual.h>
#include <heap.h>
#include <cpu.h>
#include <log.h>
#include <debug.h>
#include <assert.h>
#include <timer.h>
#include <irql.h>
#include <schedule.h>
#include <thread.h>
#include <panic.h>

extern void InitDbgScreen(void);

void MyTestThread(void* str) {
    while (1) {
        uint64_t current_time = GetThread()->time_used;
        uint64_t total_time = GetSystemTimer();
        int cpu_percent = current_time * 100 / total_time;
        DbgScreenPrintf("%s. %d%%, ", (const char*) str, cpu_percent);
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

    MarkTfwStartPoint(TFW_SP_ALL_CLEAR);
    LogWriteSerial("Boot successful! Kernel is completely initialised.\n");
    InitDbgScreen();
    DbgScreenPrintf("\n  NOS Kernel\n  Copyright Alex Boxall 2022-2023\n\n  %d / %d KB used\n\n  ...", GetTotalPhysKilobytes() - GetFreePhysKilobytes(), GetTotalPhysKilobytes());
    
    for (int i = 0; i < 500; ++i) {
        CreateThread(MyTestThread, "1", GetVas(), "test thread!");
        DbgScreenPrintf("%d: %d / %d KB used...\n", i, GetTotalPhysKilobytes() - GetFreePhysKilobytes(), GetTotalPhysKilobytes());
    }
    CreateThread(MyTestThread, "2", GetVas(), "test thread!");

    while (1) {
        Schedule();
    }
}