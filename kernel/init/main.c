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

/*
 * Next steps:
 * - uio
 * - virtual filesystem
 * - console driver
 * - IDE driver
 * - DemoFS driver
 * - ELF loader
 * - PS2.SYS, radix trees, drivers that export symbols, etc.
 * - running any old ring 3 program
 * - dynamic linker
 * - system call interface (KRNLAPI.LIB) 
 * - C standard library
 * - complete-enough CLI OS
 *          terminal that supports pipes, redirection and background processes
 *          cd, ls/dir, type, mkdir, rm, more, rename, copy, tree, mkfifo, pause, rmtree, rmdir, cls, copytree, link, 
 *                  ...ttyname, sleep, exit
 *          port zlib, nasm
 * - FAT32 driver
 */
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


void InitThread(void*) {
    MarkTfwStartPoint(TFW_SP_ALL_CLEAR);

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
    
    CreateThread(InitThread, NULL, GetVas(), "init1");
    StartMultitasking();
}