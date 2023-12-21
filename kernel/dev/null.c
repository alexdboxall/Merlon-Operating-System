
#include <heap.h>
#include <stdlib.h>
#include <vfs.h>
#include <log.h>
#include <assert.h>
#include <errno.h>
#include <transfer.h>
#include <sys/stat.h>
#include <dirent.h>

static int CheckOpen(struct vnode*, const char*, int) {
    return 0;
}

static int Ioctl(struct vnode*, int, void*) {
    return EINVAL;
}

static bool IsSeekable(struct vnode*) {
    return false;
}

static int IsTty(struct vnode*) {
    return false;
}

static int Read(struct vnode*, struct transfer*) {    
    return 0;
}

static int Write(struct vnode*, struct transfer*) {
    return 0;
}

static int Create(struct vnode*, struct vnode**, const char*, int, mode_t) {
    return EINVAL;
}

static uint8_t DirentType(struct vnode*) {
    return DT_CHR;
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

static int Truncate(struct vnode*, off_t) {
    return EINVAL;
}

static int Close(struct vnode*) {
    return 0;
}

static int Follow(struct vnode*, struct vnode**, const char*) {
    return ENOTDIR;
}

static int Readdir(struct vnode*, struct transfer*) {
    return EINVAL;
}

static const struct vnode_operations dev_ops = {
    .check_open     = CheckOpen,
    .ioctl          = Ioctl,
    .is_seekable    = IsSeekable,
    .is_tty         = IsTty,
    .read           = Read,
    .write          = Write,
    .close          = Close,
    .truncate       = Truncate,
    .create         = Create,
    .follow         = Follow,
    .dirent_type    = DirentType,
    .readdir        = Readdir,
    .stat           = Stat,
};

void InitNullDevice(void)
{
    AddVfsMount(CreateVnode(dev_ops), "null");
}
