#include <physical.h>
#include <virtual.h>
#include <heap.h>
#include <cpu.h>
#include <log.h>
#include <debug.h>
#include <assert.h>
#include <timer.h>
#include <irql.h>
#include <panic.h>

extern void InitDbgScreen(void);

void KernelMain(void) {
    LogWriteSerial("KernelMain: kernel is initialising...\n");

    InitTfw();
    MarkTfwStartPoint(TFW_SP_INITIAL);

    InitPhys();
    MarkTfwStartPoint(TFW_SP_AFTER_PHYS);

    InitHeap();
    InitIrql();
    InitTimer();
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
    
    while (1) {
        ;
    }
}