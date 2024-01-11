#include "krnlapi.h"
#include <errno.h>
#include <sys/mman.h>
#include <virtual.h>

void* mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset) {
    size_t virtual = (size_t) addr;

    int os_flags = 0;
    if (prot & PROT_EXEC) os_flags |= VM_EXEC;
    if (prot & PROT_READ) os_flags |= VM_READ;
    if (prot & PROT_WRITE) os_flags |= VM_WRITE;    
    if ((flags & MAP_ANONYMOUS) != 0) flags |= VM_FILE;
    if (flags & MAP_SHARED) os_flags |= VM_SHARED;

    /*
     * NOTE: VM_FIXED != VM_FIXED_VIRT. MAP_FIXED succeeds by removing existing 
     * mappings, where as VM_FIXED_VIRT will just fail in those cases.
     */
    if (flags & MAP_FIXED) {
        // can technically be emulated by getting the arch page size, then mapping
        // one page at a time using VM_FIXED_VIRT, and if it fails, then just 
        // unmap it before remapping.
        errno = ENOSYS;
        return MAP_FAILED;
    }

    if (flags & MAP_FIXED_NOREPLACE) {
        flags |= VM_FIXED_VIRT;
    }

    int res = _system_call(SYSCALL_MAPVIRT, os_flags, length, fd, offset, (size_t) &virtual);
    if (res == 0) {
        return (void*) virtual;
    } else {
        errno = res;
        return MAP_FAILED;
    }
}

int munmap(void* addr, size_t length) {
    int res = _system_call(SYSCALL_UNMAPVIRT, (size_t) addr, length, 0, 0, 0);
    if (res == 0) {
        return 0;
    } else {
        errno = res;
        return -1;
    }
}