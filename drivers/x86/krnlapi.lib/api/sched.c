#include "krnlapi.h"
#include <sched.h>
#include <errno.h>

void sched_yield(void) {
    _system_call(SYSCALL_YIELD, 0, 0, 0, 0, 0);
}