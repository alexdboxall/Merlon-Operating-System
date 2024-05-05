#pragma once

/*
* dirent.h - Directory Entries
*
*/

#include <sys/types.h>
#include <sys/stat.h>

#define _DIRENT_HAVE_D_TYPE

#define IFTODT(x) (x >> 15)
#define DTTOIF(x) (x << 15)

#define DT_UNKNOWN  0
#define DT_REG      IFTODT(S_IFREG)
#define DT_DIR      IFTODT(S_IFDIR)
#define DT_FIFO     IFTODT(S_IFIFO)
#define DT_SOCK     IFTODT(S_IFSOCK)
#define DT_CHR      IFTODT(S_IFCHR)
#define DT_BLK      IFTODT(S_IFBLK)
#define DT_LNK      IFTODT(S_IFLNK)

struct dirent {
    ino_t d_ino;
    char d_name[256];
    uint8_t d_namlen;
    uint8_t d_type;
    size_t d_disk;
};
