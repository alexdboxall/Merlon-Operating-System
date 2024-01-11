#include <syscall.h>
#include <errno.h>
#include <_syscallnum.h>
#include <thread.h>
#include <process.h>
#include <virtual.h>
#include <filedes.h>

int SysPrepExec(size_t, size_t, size_t, size_t, size_t) {
	int res = HandleFileDescriptorsOnExec(GetFileDescriptorTable(GetProcess()));
	if (res != 0) {
		return EINVAL;
	}

	res = UnmapVirtEx(GetVas(), ARCH_USER_AREA_BASE, ARCH_USER_AREA_LIMIT - ARCH_USER_AREA_BASE, VMUN_ALLOW_NON_EXIST);
	if (res != 0) {
		return EUNRECOVERABLE;
	}

	return 0;
}
