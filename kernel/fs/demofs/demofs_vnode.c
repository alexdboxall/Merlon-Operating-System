
#include <heap.h>
#include <log.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <vfs.h>
#include <transfer.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fs/demofs/demofs_private.h>

struct vnode_data {
    ino_t inode;
    struct demofs fs;
    uint32_t file_length;
    bool directory;
};

static int CheckOpen(struct vnode*, const char* name, int flags) {
    if (strlen(name) >= MAX_NAME_LENGTH) {
        return ENAMETOOLONG;
    }

    if ((flags & O_ACCMODE) == O_WRONLY || (flags & O_ACCMODE) == O_RDWR) {
        return EROFS;
    }

    return 0;
}

static int Ioctl(struct vnode*, int, void*) {
    return EINVAL;
}

static int Read(struct vnode* node, struct transfer* io) {    
    struct vnode_data* data = node->data;
    if (data->directory) {
        return demofs_read_directory_entry(&data->fs, data->inode, io);
    } else {
        return demofs_read_file(&data->fs, data->inode, data->file_length, io);
    }
}

static int Write(struct vnode*, struct transfer*) {
    return EROFS;
}

static int Create(struct vnode*, struct vnode**, const char*, int, mode_t) {
    return EROFS;
}

static int Truncate(struct vnode*, off_t) {
    return EROFS;
}

static int Close(struct vnode* node) {
    FreeHeap(node->data);
    return 0;
}

static struct vnode* CreateDemoFsVnode(ino_t, off_t);

static int Follow(struct vnode* node, struct vnode** out, const char* name) {
    struct vnode_data* data = node->data;
    if (data->directory) {
        ino_t child_inode;
        uint32_t file_length;

        int status = demofs_follow(&data->fs, data->inode, &child_inode, name, &file_length);
        if (status != 0) {
            return status;
        }
        
        /*
        * TODO: return existing vnode if someone opens the same file twice...
        */
    
        struct vnode* child_node = CreateDemoFsVnode(child_inode, file_length);
        struct vnode_data* child_data = AllocHeap(sizeof(struct vnode_data));
        child_data->inode = child_inode;
        child_data->fs = data->fs;
        child_data->file_length = file_length;
        child_data->directory = INODE_IS_DIR(child_inode);
        child_node->data = child_data;

        *out = child_node;

        return 0;

    } else {
        return ENOTDIR;
    }
}

static const struct vnode_operations dev_ops = {
    .check_open     = CheckOpen,
    .ioctl          = Ioctl,
    .read           = Read,
    .write          = Write,
    .close          = Close,
    .truncate       = Truncate,
    .create         = Create,
    .follow         = Follow,
};

static struct vnode* CreateDemoFsVnode(ino_t inode, off_t size) {
    return CreateVnode(dev_ops, (struct stat) {
        .st_mode = (INODE_IS_DIR(inode) ? S_IFDIR : S_IFREG) | S_IRWXU | S_IRWXG | S_IRWXO,
        .st_nlink = 1,
        .st_size = size,
        .st_ino = inode,
        .st_blksize = 512,      // the 'efficient' size
    });
}

static int CheckForDemofsSignature(struct open_file* raw_device) {
    struct stat st = raw_device->node->stat;

    uint8_t* buffer = AllocHeapEx(st.st_blksize, HEAP_ALLOW_PAGING);
    struct transfer io = CreateKernelTransfer(buffer, st.st_blksize, 8 * st.st_blksize, TRANSFER_READ);
    int res = ReadFile(raw_device, &io);
    if (res != 0) {
        FreeHeap(buffer);
        return ENOTSUP;
    }

    /*
     * Check for the DemoFS signature.
     */
    if (buffer[0] != 'D' || buffer[1] != 'E' || buffer[2] != 'M' || buffer[3] != 'O') {
        FreeHeap(buffer);
        return ENOTSUP;
    }

    return 0;
}

int DemofsMountCreator(struct open_file* raw_device, struct open_file** out) {   
	int sig_check = CheckForDemofsSignature(raw_device);
    if (sig_check != 0) {
        return sig_check;
    }
    
	struct vnode* node = CreateDemoFsVnode(9 | (1 << 31), 0);
    struct vnode_data* data = AllocHeap(sizeof(struct vnode_data));
    
    data->fs.disk = raw_device;
    data->fs.root_inode = 9 | (1 << 31);
    data->inode = 9 | (1 << 31);        /* root directory inode */
    data->file_length = 0;              /* root directory has no length */
    data->directory = true;

    node->data = data;

	*out = CreateOpenFile(node, 0, 0, true, false);
    return 0;
}