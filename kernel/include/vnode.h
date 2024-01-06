#pragma once

#include <common.h>
#include <sys/types.h>
#include <transfer.h>
#include <spinlock.h>
#include <sys/stat.h>

struct vnode;

/*
* Operations which can be performed on an abstract file. They may be left NULL,
* in this case, a default return value is supplied.
*
*   check_open: default 0
*           Called just before a file is opened to ensure that the flags and the filename
*           are valid. Flags that can be passed in are O_RDONLY, O_WRONLY and O_RDWR, and
*           O_NONBLOCK. A filename may be invalid if the name is too long for the filesystem,
*           or if the filesystem contains other reserved characters.
*
*   read: default EINVAL
*           Reads data from the file. If the file gives DT_DIR when asked for
*           dirent_type, then it should read in chunks of sizeof(struct dirent),
*           with the last being full of null bytes.
*
*   write: default EINVAL
*           Writes data to the file. Fails on directories (EISDIR).
*
*   ioctl: default EINVAL
*           Performs a miscellaneous operation on a file.
*
*   is_seekable: default EINVAL
*           Returns true if seek can be called on the file.
*
*   check_tty: default ENOTTY
*           Returns 0 if a terminal, or ENOTTY otherwise.
*
*   close: default 0
*           Frees the vnode, as its reference count has hit zero.
*
*   truncate: default EINVAL
*           Truncates the file to the given size. Fails on directories (EISDIR).
*
*   create: default EINVAL
*           Creates a new file under a given parent, with a given name. 
*           The flags specifies O_RDWR, O_RDONLY, O_WRONLY, O_EXCL and O_APPEND.
*
*   follow: default ENOTDIR
*           Returns the vnode associated with a child of the current vnode.
*           Fails on files (ENOTDIR).
*/

#define VNODE_WAIT_READ             (1 << 0)
#define VNODE_WAIT_WRITE            (1 << 1)
#define VNODE_WAIT_ERROR            (1 << 2)
#define VNODE_WAIT_HAVE_TIMEOUT     (1 << 3)
#define VNODE_WAIT_NON_BLOCK        (1 << 4)

struct vnode_operations {
    int (*check_open)(struct vnode* node, const char* name, int flags);
    int (*read)(struct vnode* node, struct transfer* io);
    int (*write)(struct vnode* node, struct transfer* io);
    int (*ioctl)(struct vnode* node, int command, void* buffer);
    int (*close)(struct vnode* node);                       // release the fileystem specific data
    int (*truncate)(struct vnode* node, off_t offset);
    int (*create)(struct vnode* node, struct vnode** out, const char* name, int flags, mode_t mode);
    int (*follow)(struct vnode* node, struct vnode** out, const char* name);
    int (*stat)(struct vnode* node, struct stat* st);
    int (*check_tty)(struct vnode* node);
    int (*wait)(struct vnode* node, int flags, uint64_t timeout_ms);

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
int VnodeOpWrite(struct vnode* node, struct transfer* io);
int VnodeOpIoctl(struct vnode* node, int command, void* buffer);
bool VnodeOpIsSeekable(struct vnode* node);
int VnodeOpCheckTty(struct vnode* node);
int VnodeOpClose(struct vnode* node);
int VnodeOpTruncate(struct vnode* node, off_t offset);
uint8_t VnodeOpDirentType(struct vnode* node);
int VnodeOpCreate(struct vnode* node, struct vnode** out, const char* name, int flags, mode_t mode);
int VnodeOpFollow(struct vnode* node, struct vnode** out, const char* name);
int VnodeOpStat(struct vnode* node, struct stat* st);
int VnodeOpWait(struct vnode* node, int flags, uint64_t timeout_ms);