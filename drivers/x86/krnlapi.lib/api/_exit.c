#include "krnlapi.h"
#include <errno.h>

void _exit(int status) {
    _system_call(SYSCALL_EXIT, status, 0, 0, 0, 0);
    while (true) {
        ;
    }
}