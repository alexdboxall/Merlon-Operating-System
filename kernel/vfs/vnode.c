#include <vnode.h>
#include <spinlock.h>
#include <assert.h>
#include <log.h>
#include <spinlock.h>
#include <errno.h>
#include <heap.h>
#include <dirent.h>
#include <irql.h>

/*
* Allocate and initialise a vnode. The reference count is initialised to 1.
*/
struct vnode* CreateVnode(struct vnode_operations ops) {
    struct vnode* node = AllocHeap(sizeof(struct vnode));
    node->ops = ops;
    node->reference_count = 1;
    node->data = NULL;
    InitSpinlock(&node->reference_count_lock, "vnoderefcnt", IRQL_SCHEDULER);
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


int VnodeOpCheckOpen(struct vnode* node, const char* name, int flags) {
    CheckVnode(node);
    if (node->ops.check_open == NULL) {
        return 0;
    }
    return node->ops.check_open(node, name, flags);
}

int VnodeOpRead(struct vnode* node, struct transfer* io) {
    CheckVnode(node);
    if (node->ops.read == NULL || io->direction != TRANSFER_READ) {
        return EINVAL;
    }
    return node->ops.read(node, io);
}

int VnodeOpWrite(struct vnode* node, struct transfer* io) {
    CheckVnode(node);
    if (node->ops.write == NULL || io->direction != TRANSFER_WRITE) {
        return EINVAL;
    }
    return node->ops.write(node, io);
}

int VnodeOpIoctl(struct vnode* node, int command, void* buffer) {
    CheckVnode(node);
    if (node->ops.ioctl == NULL) {
        return EINVAL;
    }
    return node->ops.ioctl(node, command, buffer);
}

bool VnodeOpIsSeekable(struct vnode* node) {
    CheckVnode(node);
    if (node->ops.is_seekable == NULL) {
        return false;
    }
    return node->ops.is_seekable(node);
}

int VnodeOpCheckTty(struct vnode* node) {
    CheckVnode(node);
    if (node->ops.check_tty == NULL) {
        return ENOTTY;
    }
    return node->ops.check_tty(node);
}

int VnodeOpClose(struct vnode* node) {
    /*
     * Don't call CheckVnode, as that tests the reference count being non-zero,
     * but it should be zero here.
     */
    if (node->reference_count != 0) {
        return EINVAL;
    }
    if (node->ops.close == NULL) {
        return 0;
    }
    return node->ops.close(node);
}

int VnodeOpCreate(struct vnode* node, struct vnode** out, const char* name, int flags, mode_t mode) {
    CheckVnode(node);
    if (node->ops.create == NULL) {
        return EINVAL;
    }
    return node->ops.create(node, out, name, flags, mode);
}

int VnodeOpTruncate(struct vnode* node, off_t offset) {
    CheckVnode(node);
    if (node->ops.truncate == NULL) {
        return EINVAL;
    }
    return node->ops.truncate(node, offset);
}

int VnodeOpFollow(struct vnode* node, struct vnode** new_node, const char* name) {
    CheckVnode(node);
    if (node->ops.follow == NULL) {
        return ENOTDIR;
    }
    return node->ops.follow(node, new_node, name);
}

int VnodeOpStat(struct vnode* node, struct stat* stat) {
    CheckVnode(node);
    if (node->ops.stat == NULL) {
        return EINVAL;
    }
    return node->ops.stat(node, stat);
}

uint8_t VnodeOpDirentType(struct vnode* node) {
    struct stat st;
    if (VnodeOpStat(node, &st)) {
        return DT_UNKNOWN;
    }
    return IFTODT(st.st_mode);
}

int VnodeOpWait(struct vnode* node, int flags, uint64_t timeout_ms) {
    CheckVnode(node);
    if (node->ops.wait == NULL) {
        return 0;
    }
    return node->ops.wait(node, flags, timeout_ms);
}