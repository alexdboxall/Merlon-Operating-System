#pragma once

#include <common.h>
#include <sys/types.h>
#include <transfer.h>
#include <spinlock.h>
#include <sys/stat.h>

struct vnode;

/*
* Operations which can be performed on an abstract file.
*
*   check_open
*           Called just before a file is opened to ensure that the flags and the filename
*           are valid. Flags that can be passed in are O_RDONLY, O_WRONLY and O_RDWR.
*           A filename may be invalid if the name is too long for the filesystem, or if the 
*           filesystem contains other reserved characters.
*
*   read
*           Reads data from the file. Fails on directories (EISDIR).
*
*   readdir
*           Reads a directory entry into a struct dirent. The offset in the struct transfer
*           will be a multiple of sizeof(struct dirent), and determines which directory
*           entry to use. If the offset is beyond the end of the directory, no transfer
*           should occur, and thus the bytes remaining should still be sizeof(struct dirent).
*
*   write
*           Writes data to the file. Fails on directories (EISDIR).
*
*   ioctl
*           Performs a miscellaneous operation on a file.
*
*   is_seekable
*           Returns true if seek can be called on the file.
*
*   close
*           Frees the vnode, as its reference count has hit zero.
*
*   truncate
*           Truncates the file to the given size. Fails on directories (EISDIR).
*
*   create
*           Creates a new file under a given parent, with a given name. 
*           The flags specifies O_RDWR, O_RDONLY, O_WRONLY, O_EXCL and O_APPEND.
*
*   follow
*           Returns the vnode associated with a child of the current vnode.
*           Fails on files (ENOTDIR).
*
*   dirent_type
*           Returns the type of file, either DT_DIR, DT_REG, DT_BLK, DT_CHR, DT_FIFO, DT_LNK, or DT_UNKNOWN.
*
*/

struct vnode_operations {
    int (*check_open)(struct vnode* node, const char* name, int flags);
    int (*read)(struct vnode* node, struct transfer* io);
    int (*readdir)(struct vnode* node, struct transfer* io);
    int (*write)(struct vnode* node, struct transfer* io);
    int (*ioctl)(struct vnode* node, int command, void* buffer);
    int (*close)(struct vnode* node);                       // release the fileystem specific data
    int (*truncate)(struct vnode* node, off_t offset);
    int (*create)(struct vnode* node, struct vnode** out, const char* name, int flags, mode_t mode);
    int (*follow)(struct vnode* node, struct vnode** out, const char* name);
    int (*stat)(struct vnode* node, struct stat* st);
    int (*is_tty)(struct vnode* node);

    bool (*is_seekable)(struct vnode* node);
    uint8_t (*dirent_type)(struct vnode* node);
};

struct vnode {
    struct vnode_operations ops;
    void* data;
    int reference_count;
    struct spinlock reference_count_lock;
};


/*
* Allocates a new vnode for a given set of operations.
*/
struct vnode* CreateVnode(struct vnode_operations ops);
void ReferenceVnode(struct vnode* node);
void DereferenceVnode(struct vnode* node);

/* 
* Wrapper functions to check the vnode is valid, and then call the driver.
*/
int VnodeOpCheckOpen(struct vnode* node, const char* name, int flags);
int VnodeOpRead(struct vnode* node, struct transfer* io);
int VnodeOpReaddir(struct vnode* node, struct transfer* io);
int VnodeOpWrite(struct vnode* node, struct transfer* io);
int VnodeOpIoctl(struct vnode* node, int command, void* buffer);
bool VnodeOpIsSeekable(struct vnode* node);
int VnodeOpIsTty(struct vnode* node);
int VnodeOpClose(struct vnode* node);
int VnodeOpTruncate(struct vnode* node, off_t offset);
uint8_t VnodeOpDirentType(struct vnode* node);
int VnodeOpCreate(struct vnode* node, struct vnode** out, const char* name, int flags, mode_t mode);
int VnodeOpFollow(struct vnode* node, struct vnode** out, const char* name);
int VnodeOpStat(struct vnode* node, struct stat* st);