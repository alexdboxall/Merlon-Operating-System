#include "progload.h"
#include <_syscallnum.h>
#include <fcntl.h>

int ProgramLoader(void*) {
    const char msg[] = "Message from userspace!\n";
    size_t fd;
    size_t br;
    _SystemCall(SYSCALL_OPEN, (size_t) "con:", O_WRONLY, 0, (size_t) &fd, 0);
    _SystemCall(SYSCALL_WRITE, fd, sizeof(msg), (size_t) msg, (size_t) &br, 0);
    _SystemCall(SYSCALL_CLOSE, fd, 0, 0, 0, 0);
    while (true) {
        _SystemCall(SYSCALL_YIELD, 0, 0, 0, 0, 0);
    }
}