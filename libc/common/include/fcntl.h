#pragma once

#define O_CREAT		4
#define O_EXCL		8
#define O_NOCTTY	16
#define O_TRUNC		32
#define O_APPEND	64
#define O_NONBLOCK  128
#define O_CLOEXEC   256
#define O_DIRECT    512

/*
 * Techincally, FD_CLOEXEC and O_CLOEXEC are different things: FD_CLOEXEC is a flag 
 * that a file descriptor can have, and O_CLOEXEC is passed to open() to set that flag.
 * But we can just treat them as the same thing.
 */
#define FD_CLOEXEC  O_CLOEXEC

/*
* O_RDONLY and O_WRONLY are *NOT* bitflags.
*/
#define O_RDONLY	0
#define O_WRONLY	1
#define O_RDWR		2
#define O_ACCMODE	3

#ifndef COMPILE_KERNEL

#include <sys/types.h>

#define F_DUPFD        0
#define F_GETFD        1
#define F_SETFD        2

int open(const char* filename, int flags, mode_t mode);
int creat(const char* fileame, mode_t mode);
int fcntl(int fd, int command);

#endif