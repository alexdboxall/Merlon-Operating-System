#include <syscall.h>
#include <errno.h>
#include <_syscallnum.h>
#include <thread.h>
#include <transfer.h>
#include <process.h>
#include <filedes.h>
#include <virtual.h>
#include <_stdckdint.h>
#include <sys/mman.h>

int SysMprotect(size_t virtual, size_t bytes, size_t flags, size_t, size_t) {
	if (virtual & (ARCH_PAGE_SIZE - 1)) {
		return EINVAL;
	}

	if (virtual < ARCH_USER_AREA_BASE) {
		return ENOMEM;
	}

	size_t end_of_virtual;
	bool overflow = ckd_add(&end_of_virtual, virtual, bytes);

	if (overflow || (end_of_virtual >= ARCH_USER_AREA_LIMIT)) {
		return ENOMEM;
	}

	if (flags & ~(VM_READ | VM_WRITE | VM_EXEC)) {
		return EINVAL;
	}
	
	size_t pages = BytesToPages(bytes);
	for (size_t i = 0; i < pages; ++i) {
		int res = SetVirtPermissions(virtual + i * ARCH_PAGE_SIZE, flags, VM_READ | VM_WRITE | VM_EXEC);
		if (res != 0) {
			return res;
		}
	}

	return 0;
}
