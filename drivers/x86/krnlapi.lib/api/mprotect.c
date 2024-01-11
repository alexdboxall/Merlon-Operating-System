#include "krnlapi.h"
#include <errno.h>
#include <sys/mman.h>
#include <virtual.h>

int mprotect(void* addr, size_t len, int prot) {
	int os_flags = 0;
	if (prot & PROT_EXEC) os_flags |= VM_EXEC;
	if (prot & PROT_WRITE) os_flags |= VM_WRITE;
	if (prot & PROT_READ) os_flags |= VM_READ;

    int res = _system_call(SYSCALL_MPROTECT, (size_t) addr, len, os_flags, 0, 0);
    if (res == 0) {
        return 0;
    } else {
        errno = res;
        return -1;
    }
}