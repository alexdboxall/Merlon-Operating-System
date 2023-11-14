#include <physical.h>
#include <virtual.h>
#include <heap.h>
#include <cpu.h>
#include <log.h>
#include <debug.h>
#include <panic.h>

void KernelMain(void) {
    LogWriteSerial("KernelMain: kernel is initialising...\n");

    InitTfw();
    MarkTfwStartPoint(TFW_SP_INITIAL);

    InitPhys();
    MarkTfwStartPoint(TFW_SP_AFTER_PHYS);

    InitHeap();
    MarkTfwStartPoint(TFW_SP_AFTER_HEAP);

    InitBootstrapCpu();
    MarkTfwStartPoint(TFW_SP_AFTER_BOOTSTRAP_CPU);

    InitVirt();
    MarkTfwStartPoint(TFW_SP_AFTER_VIRT);

    //ReinitPhys();
    MarkTfwStartPoint(TFW_SP_AFTER_HEAP_REINIT);

    InitOtherCpu();
    MarkTfwStartPoint(TFW_SP_AFTER_ALL_CPU);

    Panic(PANIC_MANUALLY_INITIATED);

    while (1) {
        ;
    }
}