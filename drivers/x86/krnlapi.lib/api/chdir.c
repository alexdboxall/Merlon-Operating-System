#include "krnlapi.h"
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

int fchdir(int fd) {
    int res = _system_call(SYSCALL_CHDIR, fd, 0, 0, 0, 0);
    if (res == 0) {
        return 0;
    } else {
        errno = res;
        return -1;
    }
}

int chdir(const char* path) {
    int fd = open(path, O_RDONLY, 0);
    if (fd == -1) {
        return -1;
    }
    
    int res = fchdir(fd);
    close(fd);
    return res;
}
