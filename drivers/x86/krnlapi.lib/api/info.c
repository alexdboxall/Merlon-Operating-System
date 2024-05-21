#include "krnlapi.h"
#include <errno.h>
#include <sys/mman.h>
#include <virtual.h>
#include <syscall.h>

size_t OsGetFreeMemoryKilobytes(void) {
    size_t kilobytes;
    int res = _system_call(SYSCALL_INFO, SYSINFO_FREE_RAM_KB, (size_t) &kilobytes, 0, 0, 0);
    if (res != 0) {
        return 0;
    } else {
        return kilobytes;
    }
}

size_t OsGetTotalMemoryKilobytes(void) {
    size_t kilobytes;
    int res = _system_call(SYSCALL_INFO, SYSINFO_TOTAL_RAM_KB, (size_t) &kilobytes, 0, 0, 0);
    if (res != 0) {
        return 0;
    } else {
        return kilobytes;
    }
}