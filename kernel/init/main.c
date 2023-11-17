#include <physical.h>
#include <virtual.h>
#include <heap.h>
#include <cpu.h>
#include <log.h>
#include <debug.h>
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

    InitIrql();
    InitBootstrapCpu();
    MarkTfwStartPoint(TFW_SP_AFTER_BOOTSTRAP_CPU);

    InitVirt();
    MarkTfwStartPoint(TFW_SP_AFTER_VIRT);

    //ReinitPhys();
    //RestoreHeap();
    MarkTfwStartPoint(TFW_SP_AFTER_PHYS_REINIT);

    //InitOtherCpu();
    MarkTfwStartPoint(TFW_SP_AFTER_ALL_CPU);

    PanicEx(PANIC_MANUALLY_INITIATED, "boot successful!!");

    while (1) {
        ;
    }
}