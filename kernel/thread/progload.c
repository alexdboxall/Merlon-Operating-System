
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

static size_t program_loader_addr;
static off_t program_loader_size;

void InitProgramLoader(void) {
    struct open_file* file;
    if (OpenFile("sys:/krnlapi.lib", O_RDONLY, 0, &file)) {
        PanicEx(PANIC_PROGRAM_LOADER, "krnlapi.lib couldn't be loaded");
    }

    program_loader_size = file->node->stat.st_size;
    program_loader_addr = MapVirt(0, 0, program_loader_size, VM_READ | VM_FILE, file, 0);
    CloseFile(file);
}

int LoadProgramLoaderIntoAddressSpace(size_t* entry_point) {
    int res = ArchLoadProgramLoader((void*) program_loader_addr, entry_point);
    LogWriteSerial("loading the program loader returned %d\n", res);
    return res;
}