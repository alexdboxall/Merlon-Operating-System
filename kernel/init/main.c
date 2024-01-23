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
#include <bootloader.h>
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
 * - fork
 * - dynamic libraries (e.g. c.lib)
 * - C standard library
 * - getcwd / chdir
 * - complete-enough CLI OS
 *          terminal that supports pipes, redirection and background processes
 *          cd, ls/dir, type, mkdir, rm, more, rename, copy, tree, mkfifo, 
 *             pause, rmtree, rmdir, cls, copytree, link, ttyname, sleep, exit
 *          port zlib, nasm
 * 
 * - floppy driver
 * - FAT32 driver
 * - floating point support (init and task switching)
 * - disk caching
 * - shutdown needs to close the entire VFS tree (e.g. so buffers can be 
 *              flushed, etc).
 * - recycling vnodes if opening same file more than once (required by st_nlink)
 *      -> probably a VFS-wide solution that uses AVL trees to cache the results 
 *         of follow?
 *      -> obviously need a way for FS drivers to keep it in sync, e.g. on 
 *         deletion, ioctl, etc.
 * - initrd and boot system
 * - more syscalls
 * - signals
 * - document exactly what conditions need to be checked in the vnode_ops layer,
 *      and which ones are taken care of by the VFS layer, so we don't get 
 *      people checking the same thing twice
 * - check all E... return codes... 
 * - VnodeOpWait, select/poll syscalls
 * - everyone create vnodes and open files willy-nilly - check the reference 
 *      counting, especially on closing is all correct (especially around the 
 *      virtual memory manager...). does CloseFile do what you expect??
 * - MAP_FIXED
 */

void InitUserspace(void) {
    size_t free = GetFreePhysKilobytes();
    size_t total = GetTotalPhysKilobytes();
    DbgScreenPrintf(
        "\n\nNOS Kernel\n"
        "Copyright Alex Boxall 2022-2024\n\n"
        "%d / %d KB used (%d%% free)\n\n", 
        total - free, total, 100 * (free) / total
    );
    CreateUsermodeProcess(NULL, "sys:/init.exe");
}

void InitSystemMounts(void) {
    struct file* sys_folder;
    int res = OpenFile("drv0:/System", O_RDONLY, 0, &sys_folder);
    if (res != 0) {
        PanicEx(PANIC_NO_FILESYSTEM, "sys A");
    }
    res = AddVfsMount(sys_folder->node, "sys");
    if (res != 0) {
        PanicEx(PANIC_NO_FILESYSTEM, "sys B");
    }

    struct file* swapfile;
    res = OpenFile("raw-hd0:/part1", O_RDWR, 0, &swapfile);
    if (res != 0) {
        PanicEx(PANIC_NO_FILESYSTEM, "swapfile A");
    }
    res = AddVfsMount(swapfile->node, "swap");
    if (res != 0) {
        PanicEx(PANIC_NO_FILESYSTEM, "swapfile B");
    }
}

#include <machine/portio.h>
static void InitSerialDebugging(void) {
    const int PORT = 0x3F8;
    outb(PORT + 1, 0x00);
    outb(PORT + 3, 0x80);
    outb(PORT + 0, 0x03);
    outb(PORT + 1, 0x00);
    outb(PORT + 3, 0x03);
    outb(PORT + 2, 0xC7);
    outb(PORT + 4, 0x0B);
    outb(PORT + 4, 0x1E);
    outb(PORT + 0, 0xAE);

    if(inb(PORT + 0) != 0xAE) {
        return;
    }

    outb(PORT + 4, 0x0F);
}

void InitThread(void*) {
    InitRandomDevice();
    InitNullDevice();
    InitConsole();
    InitProcess();
    InitDiskCaches();
    InitFilesystemTable();
    ArchInitDev(false);
    InitSystemMounts();
    InitSwapfile();
    InitSymbolTable();
    ArchInitDev(true);
    InitProgramLoader();
    InitUserspace();
    MarkTfwStartPoint(TFW_SP_ALL_CLEAR);

    while (true) {
        /*
         * We crash in strange and rare conditions if this thread's stack gets 
         * removed, so we will ensure we don't terminate it.
         */
        SleepMilli(100000);
    }
}

void KernelMain(struct kernel_boot_info* boot_info) {
    InitSerialDebugging();
    LogWriteSerial("KernelMain: kernel is initialising...\n");

    InitCpuTable();
    InitTfw();
    InitPhys(boot_info);
    InitIrql();
    InitVfs();
    InitTimer();
    InitScheduler();    
    InitDiskUtil();
    InitHeap();
    InitBootstrapCpu();
    InitVirt();
    ReinitPhys();
    InitOtherCpu();
    CreateThreadEx(
        InitThread, NULL, GetVas(), "init", NULL, 
        SCHEDULE_POLICY_FIXED, FIXED_PRIORITY_KERNEL_NORMAL, 0
    );
    StartMultitasking();
}
