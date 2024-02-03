#pragma once

#include <stdint.h>
#include <stddef.h>

#define TCGETS  0
#define TCSETS  1
#define TCSETSW 2
#define TCSETSF 3

#ifndef COMPILE_KERNEL
int ioctl(int fd, int cmd, ...);
#endif