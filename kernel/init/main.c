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
#include <dev.h>
#include <vfs.h>
#include <transfer.h>
#include <fcntl.h>
#include <console.h>
#include <radixtrie.h>
#include <string.h>

/*
 * Next steps:
 * - IDE driver
 * - DemoFS/FAT32 driver
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
 */
extern void InitDbgScreen(void);

void InitThread(void*) {
    DbgScreenPrintf("\n\n\n  NOS Kernel\n  Copyright Alex Boxall 2022-2023\n\n  %d / %d KB used\n\n", GetTotalPhysKilobytes() - GetFreePhysKilobytes(), GetTotalPhysKilobytes());
    MarkTfwStartPoint(TFW_SP_ALL_CLEAR);
}

#define RADIX_INSERT(k, v) key = RadixTrieCreateBoolListFromData64(k); RadixTrieInsert(trie, &key, v)
#define RADIX_GET_AND_PRINT(k) key = RadixTrieCreateBoolListFromData64(k); PutsConsole("Key: "); PutsConsole(k); PutsConsole(" Value: "); LogWriteSerial("RADIX TREE GET: 0x%X\n", RadixTrieGet(trie, &key)); PutsConsole(RadixTrieGet(trie, &key) == NULL ? "NULL" : RadixTrieGet(trie, &key)); PutsConsole("\n")
static void DummyAppThread(void*) {
	extern void InitPs2(void);
	InitPs2();
	
	PutsConsole("  C:/> ");

    struct open_file* con;
    OpenFile("con:", O_RDONLY, 0, &con);

    while (true) {
        char bf[302];
		memset(bf, 0, 302);
        struct transfer tr = CreateKernelTransfer(bf, 301, 0, TRANSFER_READ);
		ReadFile(con, &tr);
		PutsConsole("  Command not found: ");
		PutsConsole(bf);
		PutsConsole("\n  C:/> ");

        struct radix_trie* trie = RadixTrieCreate();
        struct long_bool_list key;
        RADIX_INSERT("test", "test");
        RADIX_INSERT("water", "water");
        RADIX_INSERT("slow", "slow");
        RADIX_INSERT("slower", "slower");
        RADIX_INSERT("team", "team");
        RADIX_INSERT("tester", "tester");
        RADIX_INSERT("t", "t");
        RADIX_INSERT("toast", "toast");

        RADIX_GET_AND_PRINT("test");
        RADIX_GET_AND_PRINT("water");
        RADIX_GET_AND_PRINT("slow");
        RADIX_GET_AND_PRINT("slower");
        RADIX_GET_AND_PRINT("team");
        RADIX_GET_AND_PRINT("tester");
        RADIX_GET_AND_PRINT("t");
        RADIX_GET_AND_PRINT("toast");
    }
}

#include <machine/portio.h>

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
    InitVfs();
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

    InitRandomDevice();
    InitNullDevice();
    extern void InitPs2(void);
	InitPs2();
    InitDbgScreen();
    InitConsole();
    InitProcess();

    CreateThread(DummyAppThread, NULL, GetVas(), "dummy app");

    CreateThread(InitThread, NULL, GetVas(), "init");
    StartMultitasking();
}