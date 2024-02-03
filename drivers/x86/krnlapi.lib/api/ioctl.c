#include "krnlapi.h"
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <stdarg.h>

int ioctl(int fd, int cmd, ...) {
    size_t result;

    va_list ap;
    va_start(ap, cmd); 

    void* arg1 = va_arg(ap, void*);

    int res = _system_call(SYSCALL_IOCTL, fd, cmd, (size_t) arg1, 0, (size_t) &result);
    if (res == 0) {
        return result;
    } else {
        errno = res;
        return -1;
    }
}