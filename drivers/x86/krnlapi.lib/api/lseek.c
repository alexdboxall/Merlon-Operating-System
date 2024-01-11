#include "krnlapi.h"
#include <unistd.h>
#include <errno.h>

off_t lseek(int fd, off_t offset, int whence) {
    off_t val = offset;
    int res = _system_call(SYSCALL_SEEK, fd, (size_t) &val, whence, 0, 0);
    if (res == 0) {
        return val;
    } else {
        errno = res;
        return (off_t) -1;
    }
}