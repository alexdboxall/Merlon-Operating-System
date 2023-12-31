#include <syscall.h>
#include <errno.h>
#include <_syscallnum.h>
#include <thread.h>
#include <transfer.h>
#include <virtual.h>
#include <_stdckdint.h>
#include <sys/mman.h>

int SysUnmapVirt(size_t virtual, size_t bytes, size_t, size_t, size_t) {
	if (virtual < ARCH_USER_AREA_BASE) {
		return EINVAL;
	}

	size_t end_of_virtual;
	bool overflow = ckd_add(&end_of_virtual, virtual, bytes);

	if (overflow || (end_of_virtual >= ARCH_USER_AREA_LIMIT)) {
		return EINVAL;
	}

	return UnmapVirt(virtual, bytes);
}
