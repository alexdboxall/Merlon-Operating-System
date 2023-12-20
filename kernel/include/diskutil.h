#pragma once

#include <common.h>

struct vnode;
struct open_file;

#define MAX_PARTITIONS_PER_DISK 8

#define DISKUTIL_TYPE_FIXED     0
#define DISKUTIL_TYPE_FLOPPY    1
#define DISKUTIL_TYPE_OPTICAL   2
#define DISKUTIL_TYPE_REMOVABLE 3
#define DISKUTIL_TYPE_NETWORK   4
#define DISKUTIL_TYPE_VIRTUAL   5
#define DISKUTIL_TYPE_RAM       6
#define DISKUTIL_TYPE_OTHER     7

#define __DISKUTIL_NUM_TYPES    8

struct disk_partition_helper {
    struct vnode* partitions[MAX_PARTITIONS_PER_DISK];
    char* partition_names[MAX_PARTITIONS_PER_DISK];
    int num_partitions;
};

void InitDiskUtil(void);
char* GenerateNewRawDiskName(int type);
char* GenerateNewMountedDiskName();
void CreateDiskPartitions(struct open_file* disk);
void InitDiskPartitionHelper(struct disk_partition_helper* helper);

int DiskFollowHelper(struct disk_partition_helper* helper, struct vnode** out, const char* name);
int DiskCreateHelper(struct disk_partition_helper* helper, struct vnode** in, const char* name);