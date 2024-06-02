#pragma once

#include <stdint.h>
#include <stddef.h>

#define TCGETS  0
#define TCSETS  1
#define TCSETSW 2
#define TCSETSF 3

#define TIOCGPGRP   4
#define TIOCSPGRP   5

#ifndef COMPILE_KERNEL
int ioctl(int fd, int cmd, ...);
#endif