
#include <heap.h>
#include <stdlib.h>
#include <vfs.h>
#include <log.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <transfer.h>
#include <sys/stat.h>
#include <dirent.h>
#include <virtual.h>
#include <filesystem.h>

struct partition_data {
    struct open_file* fs;
    struct open_file* disk;
    int id;
    uint64_t start_byte;
    uint64_t length_bytes;
    int disk_bytes_per_sector;
    int media_type;
    bool boot;
};

static int Access(struct vnode* node, struct transfer* tr, bool write) {
    struct partition_data* partition = node->data;
   
    uint64_t start_addr = tr->offset + partition->start_byte;
    int64_t length = tr->length_remaining;
    if (tr->offset + tr->length_remaining > partition->length_bytes) {
        length = ((int64_t) partition->length_bytes) - ((int64_t) tr->offset);
    }

    struct transfer real_transfer = *tr;
    real_transfer.length_remaining = length;
    real_transfer.offset = start_addr;

    int res = (write ? WriteFile : ReadFile)(partition->disk, &real_transfer);

    uint64_t bytes_transferred = length - real_transfer.length_remaining;

    tr->offset += bytes_transferred;
    tr->length_remaining -= bytes_transferred;
    tr->address = ((uint8_t*) tr->address) + bytes_transferred;

    return res;
}

static int Read(struct vnode* node, struct transfer* tr) {   
    return Access(node, tr, false);
}

static int Write(struct vnode* node, struct transfer* tr) {
    return Access(node, tr, true);
}

static bool IsSeekable(struct vnode*) {
    return true;
}

static int Create(struct vnode* node, struct vnode** fs, const char*, int flags, mode_t mode) {
    struct partition_data* partition = node->data;
    if (partition->fs != NULL) {
        return EALREADY;
    }

    partition->fs = CreateOpenFile(*fs, flags, mode, true, true);
    return 0;
}

static int Stat(struct vnode* node, struct stat* st) {
    struct partition_data* partition = node->data;

    LogWriteSerial("calling stat on a partition... bps = %d, len = %d\n", partition->disk_bytes_per_sector, partition->length_bytes);

    st->st_mode = S_IFBLK | S_IRWXU | S_IRWXG | S_IRWXO;
    st->st_atime = 0;
    st->st_blksize = partition->disk_bytes_per_sector;
    st->st_blocks = partition->length_bytes / partition->disk_bytes_per_sector;
    st->st_ctime = 0;
    st->st_dev = 0xBABECAFE;
    st->st_gid = 0;
    st->st_ino = 0xCAFEBABE;
    st->st_mtime = 0;
    st->st_nlink = 1;
    st->st_rdev = 0xCAFEDEAD;
    st->st_size = partition->length_bytes;
    st->st_uid = 0;
    return 0;
}

static int Follow(struct vnode* node, struct vnode** out, const char* name) {
    struct partition_data* partition = node->data;

    if (!strcmp(name, "fs")) {
        if (partition->fs == NULL) {
            return EINVAL;
        }
        
        *out = partition->fs->node;
        return 0;
    }

    return EINVAL;
}

static const struct vnode_operations dev_ops = {
    .is_seekable    = IsSeekable,
    .read           = Read,
    .write          = Write,
    .create         = Create,
    .follow         = Follow,
    .stat           = Stat,
};

struct open_file* CreatePartition(struct open_file* disk, uint64_t start, uint64_t length, int id, int sector_size, int media_type, bool boot) {
    struct partition_data* data = AllocHeap(sizeof(struct partition_data));
    data->disk = disk;
    data->disk_bytes_per_sector = sector_size;
    data->id = id;
    data->length_bytes = length;
    data->start_byte = start;
    data->media_type = media_type;
    data->boot = boot;
    data->fs = NULL;

    struct vnode* node = CreateVnode(dev_ops);
    node->data = data;

    struct open_file* partition = CreateOpenFile(node, 0, 0, true, true);
    LogWriteSerial("created the partition...\n");
    MountFilesystemForDisk(partition);
    return partition;
}

struct open_file* CreateMbrPartitionIfExists(struct open_file* disk, uint8_t* mem, int index, int sector_size) {
    int offset = 0x1BE + index * 16;

    uint8_t active = mem[offset + 0];
    if (active & 0x7F) {
        return NULL;
    }

    int media_type = mem[offset + 4];

    uint32_t start_sector = mem[offset + 11];
    start_sector <<= 8;
    start_sector |= mem[offset + 10];
    start_sector <<= 8;
    start_sector |= mem[offset + 9];
    start_sector <<= 8;
    start_sector |= mem[offset + 8];

    uint32_t total_sectors = mem[offset + 15];
    total_sectors <<= 8;
    total_sectors |= mem[offset + 14];
    total_sectors <<= 8;
    total_sectors |= mem[offset + 13];
    total_sectors <<= 8;
    total_sectors |= mem[offset + 12];

    if (start_sector == 0 && total_sectors == 0) {
        return NULL;
    }

    return CreatePartition(disk, ((uint64_t) start_sector) * sector_size, ((uint64_t) total_sectors) * sector_size, index, sector_size, media_type, active & 0x80);
}

/*
 * caller to free return value.
 */
struct open_file** GetMbrPartitions(struct open_file* disk) {
    struct stat st;
    int res = VnodeOpStat(disk->node, &st);
    if (res != 0) {
        return NULL;
    }

    uint8_t* mem = (uint8_t*) MapVirt(0, 0, st.st_blksize, VM_READ | VM_FILE, disk, 0);
    if (mem == NULL) {
        return NULL;
    }

    if (mem[0x1FE] != 0x55) {
        return NULL;
    }
    if (mem[0x1FF] != 0xAA) {
        return NULL;
    }

    struct open_file** partitions = AllocHeap(sizeof(struct open_file) * 5);
    inline_memset(partitions, 0, sizeof(struct open_file) * 5);

    int partitions_found = 0;
    for (int i = 0; i < 4; ++i) {
        struct open_file* partition = CreateMbrPartitionIfExists(disk, mem, i, st.st_blksize);
        if (partition != NULL) {
            partitions[partitions_found++] = partition;
        }
    }

    UnmapVirt((size_t) mem, st.st_blksize);
    
    return partitions;
}

/*
 * null terminated array of struct vnode*
 * e.g. {vnode_ptr_1, vnode_ptr_2, vnode_ptr_3, NULL}
 */
struct open_file** GetPartitionsForDisk(struct open_file* disk) {
    struct open_file** partitions = GetMbrPartitions(disk);
    
    if (partitions == NULL) {
        // check for GPT
    }

    return partitions;
}