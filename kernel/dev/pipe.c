
/*
 * dev/pipe.c - Unnamed Pipes
 *
 * Implements unnamed pipes - including the device driver and functions for 
 * creating pipes.
 */

#include <heap.h>
#include <stdlib.h>
#include <vfs.h>
#include <log.h>
#include <assert.h>
#include <errno.h>
#include <transfer.h>
#include <sys/stat.h>
#include <dirent.h>
#include <panic.h>
#include <ksignal.h>
#include <mailbox.h>

#define PIPE_SIGNATURE 0xa306a9b939f6d794

struct pipe {
    uint64_t sig;
    struct mailbox* mbox;
    bool broken;
};

#define PIPE_SIZE 2048

static int ReadWrite(struct vnode* node, struct transfer* tr) {
    struct pipe* pipe = node->data;
    if (pipe->broken) {
        if (tr->direction == TRANSFER_READ) {
            return 0;
        } else {
            RaiseSignal(GetThread(), SIGPIPE, false);
            return EPIPE;
        }
    }
    return MailboxAccess(pipe->mbox, tr);
}

static const struct vnode_operations dev_ops = {
    .read           = ReadWrite,
    .write          = ReadWrite,
};

/*
 * Assumes the caller has already ensured that we're calling this on a S_IFIFO.
 */
void BreakPipe(struct vnode* node) {
    struct pipe* pipe = node->data;
    if (pipe->sig == PIPE_SIGNATURE) {
        pipe->broken = true;
    } else {
        PanicEx(PANIC_UNKNOWN, "some wonky driver set stat.st_mode = S_IFIFO,"
                             "but it doesn't use struct pipe internally!");
    }
}

struct vnode* CreatePipe(void)
{
    struct vnode* node = CreateVnode(dev_ops, (struct stat) {
        .st_mode = S_IFIFO | S_IRWXU | S_IRWXG | S_IRWXO,
        .st_nlink = 1,
        .st_dev = NextDevId()
    });

    struct pipe* pipe = AllocHeap(sizeof(struct pipe));
    pipe->mbox = MailboxCreate(PIPE_SIZE);
    pipe->broken = false;
    pipe->sig = PIPE_SIGNATURE;
    node->data = pipe;
    return node;
}
