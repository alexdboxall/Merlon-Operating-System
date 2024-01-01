
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
    int res = OpenFile("sys:/progload.exe", O_RDONLY, 0, &file);
    if (res != 0) {
        PanicEx(PANIC_PROGRAM_LOADER, "program loader couldn't be loaded");
    }

    res = GetFileSize(file, &program_loader_size);
    if (res != 0) { 
        PanicEx(PANIC_PROGRAM_LOADER, "program loader size couldn't be found");
    }

    program_loader_addr = MapVirt(0, 0, program_loader_size, VM_READ | VM_FILE, file, 0);
}

int CopyProgramLoaderIntoAddressSpace(void) {
    size_t mem = MapVirt(0, ARCH_PROG_LOADER_BASE, program_loader_size, VM_READ | VM_EXEC | VM_WRITE | VM_USER | VM_LOCAL, NULL, 0);
    if (mem != ARCH_PROG_LOADER_BASE) {
        return ENOMEM;
    }

    memcpy((void*) ARCH_PROG_LOADER_BASE, (void*) program_loader_addr, program_loader_size);
    return 0;
}