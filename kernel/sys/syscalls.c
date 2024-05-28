#include <syscall.h>
#include <log.h>
#include <errno.h>
#include <_syscallnum.h>

typedef int (*system_call_t)(size_t, size_t, size_t, size_t, size_t);

static const system_call_t system_call_table[_SYSCALL_NUM_ENTRIES] = {
	[SYSCALL_YIELD] 	= SysYield,
	[SYSCALL_TERMINATE] = SysTerminate,
	[SYSCALL_MAPVIRT] 	= SysMapVirt,
	[SYSCALL_UNMAPVIRT] = SysUnmapVirt,
	[SYSCALL_OPEN] 		= SysOpen,
	[SYSCALL_READWRITE] = SysReadWrite,
	[SYSCALL_CLOSE] 	= SysClose,
	[SYSCALL_SEEK] 		= SysSeek,
	[SYSCALL_DUP] 		= SysDup,
	[SYSCALL_EXIT] 		= SysExit,
	[SYSCALL_REMOVE] 	= SysRemove,
	[SYSCALL_MPROTECT] 	= SysMprotect,
	[SYSCALL_PREPEXEC] 	= SysPrepExec,
	[SYSCALL_WAITPID] 	= SysWaitpid,
	[SYSCALL_FORK] 		= SysFork,
	[SYSCALL_GETPID] 	= SysGetPid,
	[SYSCALL_GETTID] 	= SysGetTid,
	[SYSCALL_IOCTL] 	= SysIoctl,
	[SYSCALL_STAT] 		= SysStat,
	[SYSCALL_CHDIR] 	= SysChdir,
	[SYSCALL_INFO] 		= SysInfo,
	[SYSCALL_TIME] 		= SysTime,
	[SYSCALL_NANOSLEEP]	= SysNanosleep,
};

int HandleSystemCall(int call, size_t a, size_t b, size_t c, size_t d, size_t e) {
	if (call >= _SYSCALL_NUM_ENTRIES) {
		return ENOSYS;
	}
	return system_call_table[call](a, b, c, d, e);
}