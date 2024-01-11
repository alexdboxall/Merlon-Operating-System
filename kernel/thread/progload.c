
#include <thread.h>
#include <progload.h>
#include <assert.h>
#include <string.h>
#include <irql.h>
#include <fcntl.h>
#include <log.h>
#include <errno.h>
#include <virtual.h>
#include <panic.h>
#include <common.h>
#include <semaphore.h>
#include <sys/types.h>
#include <arch.h>
#include <vfs.h>

static struct open_file* prog_loader;

void InitProgramLoader(void) {
    if (OpenFile("sys:/krnlapi.lib", O_RDONLY, 0, &prog_loader)) {
        PanicEx(PANIC_PROGRAM_LOADER, "krnlapi.lib couldn't be loaded");
    }
}

int LoadProgramLoaderIntoAddressSpace(size_t* entry_point) {
    size_t relocation_point = ARCH_PROG_LOADER_BASE;
    return ArchLoadDriver(&relocation_point, prog_loader, NULL, entry_point);
}