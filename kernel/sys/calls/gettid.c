#include <syscall.h>
#include <errno.h>
#include <_syscallnum.h>
#include <thread.h>
#include <log.h>
#include <vfs.h>
#include <process.h>
#include <filedes.h>

int SysGetTid(size_t, size_t, size_t, size_t, size_t) {
    return GetThread()->thread_id;
}
