#include "krnlapi.h"
#include <errno.h>
#include <sys/mman.h>
#include <virtual.h>
#include <syscall.h>

int OsSetLocalTime(uint64_t time) {
    return _system_call(SYSCALL_TIME, (size_t) &time, 1, 0, 0, 0);
}

uint64_t OsGetLocalTime(void) {
    uint64_t time;
    _system_call(SYSCALL_TIME, (size_t) &time, 0, 0, 0, 0);
    return time;
}

int OsSetTimezone(const char* name) {
    return _system_call(SYSCALL_TIME, (size_t) name, 3, 0, 0, 0);
}

int OsGetTimezone(char* name_out, int max_length, uint64_t* offset_out) {
    char internal_buffer[128];
    uint64_t offset;
    int res = _system_call(SYSCALL_TIME, (size_t) internal_buffer, 2, 127, (size_t) &offset, 0);
    if (res != 0) {
        return res;
    }

    if (name_out != NULL) {
        if (max_length < 18) {
            return ENAMETOOLONG;
        }
        xstrcpy(name_out, internal_buffer);
    }
    
    if (offset_out != NULL) {
        *offset_out = offset;
    }
    
    return 0;
}