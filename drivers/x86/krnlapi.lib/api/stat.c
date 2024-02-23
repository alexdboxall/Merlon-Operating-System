#include "krnlapi.h"
#include <errno.h>
#include <sys/stat.h>

static int CommonStat(const char* restrict path, struct stat* restrict buf, int fd, int link) {
    int res = _system_call(SYSCALL_STAT, (size_t) path, (size_t) buf, path == NULL, fd, link);
    if (res == 0) {
        return 0;
    } else {
        errno = res;
        return -1;
    }
}

int stat(const char* restrict path, struct stat* restrict buf) {
    return CommonStat(path, buf, 0, 0);
}

int fstat(int fd, struct stat* buf) {
    return CommonStat(NULL, buf, fd, 0);
}

int lstat(const char* restrict path, struct stat* restrict buf) {
    return CommonStat(path, buf, 0, 1);
}