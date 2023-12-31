
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
#include <vfs.h>

void InitProgramLoader(void) {
    struct open_file* file;
    int res = OpenFile("sys:/progload.exe", O_RDONLY, 0, &file);
    if (res != 0) {
        PanicEx(PANIC_PROGRAM_LOADER, "program loader couldn't be loaded");
    }

    off_t file_size;
    res = GetFileSize(file, &file_size);
    if (res != 0) { 
        PanicEx(PANIC_PROGRAM_LOADER, "program loader size couldn't be found");
    }

    size_t mem = MapVirt(0, ARCH_PROG_LOADER_BASE, file_size, VM_READ | VM_FILE | VM_USER | VM_EXEC, file, 0);
    if (mem != ARCH_PROG_LOADER_BASE) {
        PanicEx(PANIC_PROGRAM_LOADER, "program loader couldn't be loaded at the correct address");
    }
}