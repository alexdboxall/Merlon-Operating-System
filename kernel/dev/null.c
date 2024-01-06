
#include <heap.h>
#include <stdlib.h>
#include <vfs.h>
#include <log.h>
#include <assert.h>
#include <errno.h>
#include <transfer.h>
#include <sys/stat.h>
#include <dirent.h>

static int Read(struct vnode*, struct transfer*) {
    return 0;
}

static int Write(struct vnode*, struct transfer*) {
    // TODO: do we need to set io->length_remaining to zero?
    return 0;
}

static int Stat(struct vnode*, struct stat* st) {
    st->st_mode = S_IFCHR | S_IRWXU | S_IRWXG | S_IRWXO;
    st->st_atime = 0;
    st->st_blksize = 0;
    st->st_blocks = 0;
    st->st_ctime = 0;
    st->st_dev = 0xBABECAFE;
    st->st_gid = 0;
    st->st_ino = 0xCAFEBABE;
    st->st_mtime = 0;
    st->st_nlink = 1;
    st->st_rdev = 0xCAFEDEAD;
    st->st_size = 0;
    st->st_uid = 0;
    return 0;
}

static const struct vnode_operations dev_ops = {
    .read           = Read,
    .write          = Write,
    .stat           = Stat,
};

void InitNullDevice(void)
{
    AddVfsMount(CreateVnode(dev_ops), "null");
}
