#include "krnlapi.h"
#include <errno.h>
#include <unistd.h>

static ssize_t common_read_write(int fd, size_t buf, size_t count, int write) {
    size_t br;
    int res = _system_call(SYSCALL_READWRITE, fd, count, buf, (size_t)&br, write);
    if (res == 0) {
        return br;
    } else {
        errno = res;
        return -1;
    }
}

ssize_t write(int fd, const void* buf, size_t count) {
    return common_read_write(fd, (size_t) buf, count, 1);
}

ssize_t read(int fd, void* buf, size_t count) {
    return common_read_write(fd, (size_t) buf, count, 0);
}