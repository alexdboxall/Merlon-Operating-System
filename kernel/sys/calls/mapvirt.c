#include <syscall.h>
#include <errno.h>
#include <_syscallnum.h>
#include <thread.h>
#include <transfer.h>
#include <process.h>
#include <filedes.h>
#include <virtual.h>
#include <log.h>
#include <_stdckdint.h>
#include <sys/mman.h>

int SysMapVirt(size_t flags, size_t bytes, size_t fd, size_t offset, size_t userptr_virt) {
	if (flags & ~(VM_READ | VM_WRITE | VM_EXEC | VM_FILE | VM_FIXED_VIRT | VM_SHARED)) {
		return EINVAL;
	}

	size_t target_virtual;
	int res = ReadWordFromUsermode((size_t*) userptr_virt, &target_virtual);
	if (res != 0) {
		return res;
	}

	if (target_virtual != 0 && target_virtual < ARCH_USER_AREA_BASE) {
		return EINVAL;
	}

	size_t end_of_virtual;
	bool overflow = ckd_add(&end_of_virtual, target_virtual, bytes);

	if (target_virtual != 0 && (overflow || (end_of_virtual >= ARCH_USER_AREA_LIMIT))) {
		return EINVAL;
	}

	struct file* file = NULL;
	if (flags & VM_FILE) {
		res = GetFileFromFd(GetFdTable(GetProcess()), fd, &file);
		if (file == NULL || res != 0) {
			return res;
		}
	}

	int error;
	size_t output_virtual = MapVirtEx(
		GetVas(), 0, target_virtual, BytesToPages(bytes), 
		flags | VM_USER | VM_LOCAL, file, offset, &error
	);
	
	if (output_virtual == 0) {
		return error;
	}

	return WriteWordToUsermode((size_t*) userptr_virt, output_virtual);
}
