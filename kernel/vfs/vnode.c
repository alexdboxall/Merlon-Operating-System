#include <vnode.h>
#include <spinlock.h>
#include <assert.h>
#include <log.h>
#include <spinlock.h>
#include <heap.h>
#include <irql.h>

/*
* vfs/vnode.c - Virtual Filesystem Nodes
*
* Each vnode represents an abstract file, such as a file, directory or device.
*/

/*
* Allocate and initialise a vnode. The reference count is initialised to 1.
*/
struct vnode* CreateVnode(struct vnode_operations ops) {
    struct vnode* node = AllocHeap(sizeof(struct vnode));
    node->ops = ops;
    node->reference_count = 1;
    node->data = NULL;
    InitSpinlock(&node->reference_count_lock, "vnode refcnt", IRQL_SCHEDULER);

    return node;
}

/*
* Cleanup and free an abstract file node.
*/ 
static void DestroyVnode(struct vnode* node) {
    /*
    * The lock can't be held during this process, otherwise the lock will
    * get freed before it is released (which is bad, as we must release it
    * to get interrupts back on).
    */

    assert(node != NULL);
    assert(node->reference_count == 0);

    FreeHeap(node);
}

/*
* Ensures that a vnode is valid.
*/
static void CheckVnode(struct vnode* node) {
    assert(node != NULL);
    assert(node->ops.check_open != NULL);
    assert(node->ops.read != NULL);
    assert(node->ops.write != NULL);
    assert(node->ops.ioctl != NULL);
    assert(node->ops.is_seekable != NULL);
    assert(node->ops.is_tty != NULL);
    assert(node->ops.close != NULL);
    assert(node->ops.create != NULL);
    assert(node->ops.stat != NULL);
    assert(node->ops.truncate != NULL);
    assert(node->ops.follow != NULL);
    assert(node->ops.dirent_type != NULL);
    assert(node->ops.readdir != NULL);

    if (IsSpinlockHeld(&node->reference_count_lock)) {
        assert(node->reference_count > 0);
    } else {
        AcquireSpinlockIrql(&node->reference_count_lock);
        assert(node->reference_count > 0);
        ReleaseSpinlockIrql(&node->reference_count_lock);
    }
}


/*
* Increments a vnode's reference counter. Used whenever a vnode is 'given' to someone.
*/
void ReferenceVnode(struct vnode* node) {
    assert(node != NULL);

    AcquireSpinlockIrql(&node->reference_count_lock);
    node->reference_count++;
    ReleaseSpinlockIrql(&node->reference_count_lock);
}

/*
* Decrements a vnode's reference counter, destorying it if it reaches zero. 
* It should be called to free a vnode 'given' to use when it is no longer needed.
*/
void DereferenceVnode(struct vnode* node) {
    CheckVnode(node);
    AcquireSpinlockIrql(&node->reference_count_lock);

    assert(node->reference_count > 0);
    node->reference_count--;

    if (node->reference_count == 0) {
        VnodeOpClose(node);

        /*
        * Must release the lock before we delete it so we can put interrupts back on
        */
        ReleaseSpinlockIrql(&node->reference_count_lock);

        DestroyVnode(node);
        return;
    }

    ReleaseSpinlockIrql(&node->reference_count_lock);
}


/*
* Wrapper functions for performing operations on a vnode. Also
* performs validation on the vnode.
*/
int VnodeOpCheckOpen(struct vnode* node, const char* name, int flags) {
    CheckVnode(node);
    return node->ops.check_open(node, name, flags);
}

int VnodeOpRead(struct vnode* node, struct transfer* io) {
    CheckVnode(node);
    assert(io->direction == TRANSFER_READ);
    return node->ops.read(node, io);
}

int VnodeOpReaddir(struct vnode* node, struct transfer* io) {
    CheckVnode(node);
    assert(io->direction == TRANSFER_READ);
    return node->ops.readdir(node, io);
}

int VnodeOpWrite(struct vnode* node, struct transfer* io) {
    CheckVnode(node);
    assert(io->direction == TRANSFER_WRITE);
    return node->ops.write(node, io);
}

int VnodeOpIoctl(struct vnode* node, int command, void* buffer) {
    CheckVnode(node);
    return node->ops.ioctl(node, command, buffer);
}

bool VnodeOpIsSeekable(struct vnode* node) {
    CheckVnode(node);
    return node->ops.is_seekable(node);
}

int VnodeOpIsTty(struct vnode* node) {
    CheckVnode(node);
    return node->ops.is_tty(node);
}

int VnodeOpClose(struct vnode* node) {
    /*
    * Don't check for validity as the reference count is currently at zero,
    * (and thus the check will fail), and we just checked its validity in
    * DereferenceVnode.
    */
    return node->ops.close(node);
}

int VnodeOpCreate(struct vnode* node, struct vnode** out, const char* name, int flags, mode_t mode) {
    CheckVnode(node);
    return node->ops.create(node, out, name, flags, mode);
}

int VnodeOpTruncate(struct vnode* node, off_t offset) {
    CheckVnode(node);
    return node->ops.truncate(node, offset);
}

int VnodeOpFollow(struct vnode* node, struct vnode** new_node, const char* name) {
    CheckVnode(node);
    return node->ops.follow(node, new_node, name);
}

uint8_t VnodeOpDirentType(struct vnode* node) {
    CheckVnode(node);
    return node->ops.dirent_type(node);
}

int VnodeOpStat(struct vnode* node, struct stat* stat) {
    CheckVnode(node);
    return node->ops.stat(node, stat);
}
