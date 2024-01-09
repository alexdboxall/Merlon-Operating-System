
#include <heap.h>
#include <stdlib.h>
#include <vfs.h>
#include <log.h>
#include <assert.h>
#include <errno.h>
#include <transfer.h>
#include <sys/stat.h>
#include <dirent.h>

static int ReadWrite(struct vnode*, struct transfer*) {
    // TODO: do we need to set io->length_remaining to zero? 
    // (on either read or write??)
    return 0;
}

static const struct vnode_operations dev_ops = {
    .read           = ReadWrite,
    .write          = ReadWrite,
};

void InitNullDevice(void)
{
    AddVfsMount(CreateVnode(dev_ops, (struct stat) {
        .st_mode = S_IFCHR | S_IRWXU | S_IRWXG | S_IRWXO,
        .st_nlink = 1,
    }), "null");
}
