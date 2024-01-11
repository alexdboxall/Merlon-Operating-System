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

struct vnode_operations {
    int (*check_open)(struct vnode* node, const char* name, int flags);
    int (*read)(struct vnode* node, struct transfer* io);
    int (*write)(struct vnode* node, struct transfer* io);
    int (*ioctl)(struct vnode* node, int command, void* buffer);
    int (*close)(struct vnode* node);                       // release the fileystem specific data
    int (*truncate)(struct vnode* node, off_t offset);
    int (*create)(struct vnode* node, struct vnode** out, const char* name, int flags, mode_t mode);
    int (*follow)(struct vnode* node, struct vnode** out, const char* name);
    int (*wait)(struct vnode* node, int flags, uint64_t timeout_ms);

    /*
     * Must fail with EISDIR on directories. Should only decrement st.st_nlink, 
     * and remove the link from the fileystem. On things like FAT, where hard 
     * links are not supported, this can just decrement st.st_nlink, as we know
     * that ops.delete is on its way, and that can properly delete it.
     */
    int (*unlink)(struct vnode* node);

    /*
     * Deletes a file or directory from the filesystem completely. For files, 
     * the return value given will not propogate back to the VFS caller, as it 
     * gets called in DestroyVnode(). For files, st.st_nlink will be 0 on
     * call.
     * 
     * For directories, this function *must* check if the directory is non-empty
     * and fail with ENOTEMPTY if so. st.st_nlink will be 1 on call - does not 
     * need to be modified.
     */
    int (*delete)(struct vnode* node);
};

struct vnode {
    struct vnode_operations ops;
    void* data;
    int reference_count;
    struct spinlock reference_count_lock;
    struct stat stat;
};

/*
* Allocates a new vnode for a given set of operations.
*/
struct vnode* CreateVnode(struct vnode_operations ops, struct stat st);
void ReferenceVnode(struct vnode* node);
void DereferenceVnode(struct vnode* node);

/* 
* Wrapper functions to check the vnode is valid, and then call the driver.
*/
int VnodeOpCheckOpen(struct vnode* node, const char* name, int flags);
int VnodeOpRead(struct vnode* node, struct transfer* io);
int VnodeOpWrite(struct vnode* node, struct transfer* io);
int VnodeOpIoctl(struct vnode* node, int command, void* buffer);
int VnodeOpClose(struct vnode* node);
int VnodeOpTruncate(struct vnode* node, off_t offset);
uint8_t VnodeOpDirentType(struct vnode* node);
int VnodeOpCreate(struct vnode* node, struct vnode** out, const char* name, int flags, mode_t mode);
int VnodeOpFollow(struct vnode* node, struct vnode** out, const char* name);
int VnodeOpWait(struct vnode* node, int flags, uint64_t timeout_ms);
int VnodeOpUnlink(struct vnode* node);
int VnodeOpDelete(struct vnode* node);