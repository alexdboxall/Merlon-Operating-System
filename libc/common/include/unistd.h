#pragma once

#define SEEK_SET    0
#define SEEK_CUR    1
#define SEEK_END    2

#ifndef COMPILE_KERNEL

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#include <sys/types.h>

int isatty(int fd);
int close(int fd);

ssize_t read(int fd, void* buffer, size_t size);
ssize_t write(int fd, const void* buffer, size_t size);

off_t lseek(int fd, off_t offset, int whence);

int dup(int oldfd);
int dup2(int oldfd, int newfd);
int dup3(int oldfd, int newfd, int flags);

pid_t fork(void);

int execve(const char* path, char* const argv[], char* const envp[]);
int unlink(const char* path);
int rmdir(const char* path);

pid_t getpid(void);
pid_t getppid(void);
pid_t gettid(void);

int chdir(const char* path);
int fchdir(int fd);

char* getcwd(char* buf, size_t size);

#endif