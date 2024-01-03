#include "krnlapi.h"

/*
 * KRNLAPI.LIB will implement the following C functions (incomplete list):
 *
 * open, close, read, write, lseek, dup, dup2, dup3, mmap, munmap, _exit, execve, fork, spawn, fstat,
 * getpid, isatty, kill, link, stat, times, unlink, wait, opendir, readdir, mkdir, rmdir, pipe, mkfifo, sleep,
 * chdir, getcwd, truncate, sync, symlink, etc.
 * 
 * what's needed for pthreads to be implemented over the top in the C library (e.g. semaphores, mutexes, thread creation, etc.)
 * some termios functions...
 * ways of getting program arguments and environment variables...
 * 
 * NOS specific calls, e.g. power management, driver loading, timers, etc.
 *  nos_shutdown
 *  nos_reboot
 *  nos_enter_sleep_mode
 *  nos_load_driver
 *  nos_get_total_ram
 *  nos_get_free_ram
 *  nos_get_system_timer
 */

int isatty(int fd);
int close(int fd);

ssize_t read(int fd, void* buffer, size_t size);
ssize_t write(int fd, const void* buffer, size_t size);

off_t lseek(int fd, off_t offset, int whence);

pid_t fork(void);
