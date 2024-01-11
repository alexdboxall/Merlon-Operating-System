#include "krnlapi.h"
#include <errno.h>

static int common_remove(const char* path, int rmdir) {
    int res = _system_call(SYSCALL_REMOVE, (size_t) path, rmdir, 0, 0, 0);
    if (res == 0) {
        return 0;
    } else {
        errno = res;
        return -1;
    }
}

int unlink(const char* path) {
    return common_remove(path, 0);
}

int rmdir(const char* path) {
    return common_remove(path, 1);
}