#include <syscall.h>
#include <errno.h>
#include <_syscallnum.h>
#include <transfer.h>
#include <log.h>
#include <common.h>
#include <physical.h>

int SysInfo(size_t cmd, size_t result_word, size_t result_str, size_t arg, size_t) {
    switch (cmd) {
    case SYSINFO_FREE_RAM_KB:
        return WriteWordToUsermode((size_t*) result_word, GetFreePhysKilobytes());
    case SYSINFO_TOTAL_RAM_KB:
        return WriteWordToUsermode((size_t*) result_word, GetTotalPhysKilobytes());
    case SYSINFO_OS_VERSION: {
        uint64_t max_len = arg > 255 ? 255 : arg;
        int res = WriteStringToUsermode(OS_VERSION_STRING, (char*) result_str, max_len + 1);
        if (res != 0) {
            return res;
        }
        uint32_t version = (OS_VERSION_MINOR & 0xFF) | ((OS_VERSION_MAJOR & 0xFF) << 8);
        res = WriteWordToUsermode((size_t*) result_word, version);
        return res;
    }
    case SYSINFO_IS_SUPPORTED:
        return arg < _SYSINFO_NUM_CMDS ? 0 : ENOSYS;
    }
    return ENOSYS;
}


/*

#define SYSINFO_FREE_RAM_KB     0
#define SYSINFO_TOTAL_RAM_KB    1
#define SYSINFO_OS_VERSION      2
#define SYSINFO_IS_SUPPORTED    3*/
