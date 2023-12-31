#include "progload.h"
#include <_syscallnum.h>

int ProgramLoader(void*) {
    while (true) {
        _SystemCall(SYSCALL_YIELD, 0, 0, 0, 0);
    }
}