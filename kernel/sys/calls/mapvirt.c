#include <syscall.h>
#include <errno.h>
#include <_syscallnum.h>
#include <thread.h>
#include <transfer.h>
#include <virtual.h>
#include <_stdckdint.h>
#include <sys/mman.h>

int SysMapVirt(size_t flags, size_t bytes, size_t fd, size_t offset, size_t virtual_) {
	size_t* userptr_virt = (size_t*) virtual_;

	int safe_flags = VM_USER | VM_LOCAL;

	if (flags & VM_READ) {
		safe_flags |= VM_READ;
	}
	if (flags & VM_WRITE) {
		safe_flags |= VM_WRITE;
	}
	if (flags & VM_EXEC) {
		safe_flags |= VM_EXEC;
	}
	if (flags & VM_FILE) {
		safe_flags |= VM_FILE;
	}
	if (flags & VM_FIXED_VIRT) {
		safe_flags |= VM_FIXED_VIRT;
	}

	(void) fd;

	size_t target_virtual;
	int res = ReadWordFromUsermode(userptr_virt, &target_virtual);
	if (res != 0) {
		return res;
	}

	if (target_virtual < ARCH_USER_AREA_BASE) {
		return EINVAL;
	}

	size_t end_of_virtual;
	bool overflow = ckd_add(&end_of_virtual, target_virtual, bytes);

	if (overflow || (end_of_virtual >= ARCH_USER_AREA_LIMIT)) {
		return EINVAL;
	}

	size_t output_virtual = MapVirt(0, target_virtual, bytes, safe_flags, NULL, offset);
	if (output_virtual == 0) {
		return EINVAL;
	}

	return WriteWordToUsermode(userptr_virt, output_virtual);
}
