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
#include <progload.h>
#include <dev.h>
#include <vfs.h>
#include <diskcache.h>
#include <transfer.h>
#include <fcntl.h>
#include <console.h>
#include <swapfile.h>
#include <diskutil.h>
#include <string.h>
#include <filesystem.h>
#include <driver.h>

/*
 * Next steps:
 * - program loader / dynamic linker
 * - system call interface (KRNLAPI.LIB) 
 * - C standard library
 * - complete-enough CLI OS
 *          terminal that supports pipes, redirection and background processes
 *          cd, ls/dir, type, mkdir, rm, more, rename, copy, tree, mkfifo, pause, rmtree, rmdir, cls, copytree, link, 
 *                  ...ttyname, sleep, exit
 *          port zlib, nasm
 * - floppy driver
 * - FAT32 driver
 * - floating point support (init and task switching)
 * - disk caching
 * - shutdown needs to close the entire VFS tree (e.g. so buffers can be flushed, etc).
 * - recycling vnodes if opening same file more than once
 * - initrd and boot system
 * - more syscalls
 * - document exactly what conditions need to be checked in the vnode_ops layer, and which ones are taken care of by
 *      the VFS layer, so we don't get people checking the same thing twice
 * - check all E... return codes... 
 * - VnodeOpWait, select/poll syscalls
 * - everyone create vnodes and open files willy-nilly - check the reference counting, especially on closing is 
 *      all correct (especially around the virtual memory manager...). does CloseFile do what you expect??
 *      I THINK OPEN FILES SHOULD HOLD INCREMENT THE VNODE REFERENCE ON CREATION, AND DECREMENT WHEN THE OPEN FILE
 *      GOES TO ZERO.
 * 
 *      that way it will go a bit like this:
 *      
 *          after call to:      vnode refs      openfile refs
 *          Create vnode        1               0
 *          Create openfile     2               1
 *          CloseFile()         1               0 -> gets destroyed
 *               ->             0 
 * 
 *          CloseFile() closes both the openfile and the vnode.
 * 
 *     And then if the VMM gets involved...
 * 
 *          after call to:      vnode refs      openfile refs
 *          Create vnode        1               0
 *          Create openfile     2               1
 *          MapVirt(10 pgs)     2               11
 *          CloseFile()         1               10
 *          UnmapVirt(10 pgs)   1               0 -> gets destroyed, and in doing so,
 *            ...               -> 0 gets destroyed
 * 
 *    OK - that's been implemented now... now to see if it works...
 * 
 * - MAP_FIXED, MAP_SHARED
 * 
 */

 void DummyAppThread(void*) {
	PutsConsole("drv0:/> ");

    struct open_file* con;
    OpenFile("con:", O_RDONLY, 0, &con);

    while (true) {
        char bf[302];
		inline_memset(bf, 0, 302);
        struct transfer tr = CreateKernelTransfer(bf, 301, 0, TRANSFER_READ);
		ReadFile(con, &tr);
		PutsConsole("Command not found: ");
		PutsConsole(bf);
        PutsConsole("\n");

        if (bf[0] == 'u' || bf[0] == 'U') {
            CreateUsermodeProcess(NULL, "sys:/init.exe");

        } else if (bf[0] == 'p' || bf[0] == 'P') {
            Panic(PANIC_MANUALLY_INITIATED);

        } else if (bf[0] == 'e' || bf[0] == 'E') {
            MapVirt(0, 0, 8 * 4096, VM_LOCK | VM_READ, NULL, 0);
        }
        
		PutsConsole("drv0:/> ");
    }
}

void InitUserspace(void) {
    size_t free = GetFreePhysKilobytes();
    size_t total = GetTotalPhysKilobytes();
    DbgScreenPrintf("NOS Kernel\nCopyright Alex Boxall 2022-2023\n\n%d / %d KB used (%d%% free)\n\n", total - free, total, 100 * (free) / total);
    CreateThread(DummyAppThread, NULL, GetVas(), "dummy app");
}

void InitThread(void*) {
    ReinitHeap();
    InitRandomDevice();
    InitNullDevice();
    InitConsole();
    InitProcess();
    InitDiskCaches();
    
    InitFilesystemTable();
    ArchInitDev(false);

    struct open_file* sys_folder;
    int res = OpenFile("drv0:/System", O_RDONLY, 0, &sys_folder);
    if (res != 0) {
        PanicEx(PANIC_NO_FILESYSTEM, "sys A");
    }
    res = AddVfsMount(sys_folder->node, "sys");
    if (res != 0) {
        PanicEx(PANIC_NO_FILESYSTEM, "sys B");
    }

    struct open_file* swapfile;
    res = OpenFile("raw-hd0:/part1", O_RDWR, 0, &swapfile);
    if (res != 0) {
        PanicEx(PANIC_NO_FILESYSTEM, "swapfile A");
    }
    res = AddVfsMount(swapfile->node, "swap");
    if (res != 0) {
        PanicEx(PANIC_NO_FILESYSTEM, "swapfile B");
    }

    InitSwapfile();
    InitSymbolTable();
    ArchInitDev(true);
    InitProgramLoader();
    InitUserspace();

    MarkTfwStartPoint(TFW_SP_ALL_CLEAR);

    while (true) {
        /*
         * We crash in strange and rare conditions if this thread's stack gets removed, so we will
         * ensure we don't terminate it.
         */
        SleepMilli(100000);
    }
}

#include <machine/portio.h>
static void InitSerialDebugging(void) {
    const int PORT = 0x3F8;
    outb(PORT + 1, 0x00);    // Disable all interrupts
    outb(PORT + 3, 0x80);    // Enable DLAB (set baud rate divisor)
    outb(PORT + 0, 0x03);    // Set divisor to 3 (lo byte) 38400 baud
    outb(PORT + 1, 0x00);    //                  (hi byte)
    outb(PORT + 3, 0x03);    // 8 bits, no parity, one stop bit
    outb(PORT + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
    outb(PORT + 4, 0x0B);    // IRQs enabled, RTS/DSR set
    outb(PORT + 4, 0x1E);    // Set in loopback mode, test the serial chip
    outb(PORT + 0, 0xAE);    // Test serial chip (send byte 0xAE and check if serial returns same byte)

    // Check if serial is faulty (i.e: not same byte as sent)
    if(inb(PORT + 0) != 0xAE) {
        return;
    }

    // If serial is not faulty set it in normal operation mode
    // (not-loopback with IRQs enabled and OUT#1 and OUT#2 bits enabled)
    outb(PORT + 4, 0x0F);
}

void KernelMain(void) {
    InitSerialDebugging();

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

    /*
     * Allows deferments of functions to actually happen. IRQL is still usable beforehand though.
     */
    InitIrql();
    InitVfs();
    InitTimer();
    InitScheduler();
    InitDiskUtil();

    InitHeap();
    MarkTfwStartPoint(TFW_SP_AFTER_HEAP);

    InitBootstrapCpu();
    MarkTfwStartPoint(TFW_SP_AFTER_BOOTSTRAP_CPU);

    InitVirt();
    MarkTfwStartPoint(TFW_SP_AFTER_VIRT);

    ReinitPhys();
    MarkTfwStartPoint(TFW_SP_AFTER_PHYS_REINIT);

    InitOtherCpu();
    MarkTfwStartPoint(TFW_SP_AFTER_ALL_CPU);


    CreateThreadEx(InitThread, NULL, GetVas(), "init", NULL, SCHEDULE_POLICY_FIXED, FIXED_PRIORITY_KERNEL_NORMAL, 0);
    StartMultitasking();
}