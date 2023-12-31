#pragma once

/*
 * Through the system call, these will all just be combined into one set of flags, instead
 * of 2 like the mmap() wrapper will use.
 */

#define PROT_NONE       0
#define PROT_EXEC       (1 << 0)
#define PROT_READ       (1 << 1)
#define PROT_WRITE      (1 << 2)

#define MAP_SHARED      (1 << 3)
#define MAP_PRIVATE     0               /* PRIVATE is simply 'not SHARED' */
#define MAP_ANONYMOUS   (1 << 4)
#define MAP_ANON        MAP_ANONYMOUS
#define MAP_DENYWRITE   0
#define MAP_EXECUTABLE  0
#define MAP_FILE        0
#define MAP_FIXED       (1 << 5)