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

void KernelMain(void) {
    LogWriteSerial("KernelMain: kernel is initialising...\n");

    InitTfw();
    LogWriteSerial("init tfw done.\n");
    
    MarkTfwStartPoint(TFW_SP_INITIAL);

    InitPhys();
    MarkTfwStartPoint(TFW_SP_AFTER_PHYS);

    InitHeap();
    MarkTfwStartPoint(TFW_SP_AFTER_HEAP);

    /*
     * These actually just do things like allocate locks, etc., they don't actually, e.g.
     * initialise the timer hardware.
     */
    InitIrql();
    InitTimer();

    InitBootstrapCpu();
    assert(GetIrql() == IRQL_STANDARD);
    MarkTfwStartPoint(TFW_SP_AFTER_BOOTSTRAP_CPU);

    InitVirt();
    MarkTfwStartPoint(TFW_SP_AFTER_VIRT);

    //ReinitPhys();
    MarkTfwStartPoint(TFW_SP_AFTER_PHYS_REINIT);

    //InitOtherCpu();
    MarkTfwStartPoint(TFW_SP_AFTER_ALL_CPU);

    MarkTfwStartPoint(TFW_SP_ALL_CLEAR);
    LogWriteSerial("Boot successful! Kernel is completely initialised.\n");
    while (1) {
        ;
    }
}