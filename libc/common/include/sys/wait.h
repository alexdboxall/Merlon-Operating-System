#pragma once

#include <sys/types.h>

#define WNOHANG (1 << 0)

/* 
 * We don't support stopped processes, but we may as well define this flag
 * anyway, as we can technically honour it (due to lack of stopped processes).
 */
#define WUNTRACED (1 << 1)

#ifndef COMPILE_KERNEL
pid_t wait(int* status);
pid_t waitpid(pid_t pid, int* status, int options);
#endif

#define WIFEXITED(x) (x == 0)
