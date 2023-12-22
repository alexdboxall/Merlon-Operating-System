#include <diskutil.h>
#include <string.h>
#include <irql.h>
#include <assert.h>
#include <spinlock.h>
#include <vfs.h>
#include <errno.h>
#include <log.h>
#include <irql.h>
#include <partition.h>
#include <sys/stat.h>

/*
 * Stores how many disks of each type have been allocated so far.
 */
static int type_table[__DISKUTIL_NUM_TYPES];

/*
 * Protects `type_table` and `next_mounted_disk_num`
 */
static struct spinlock type_table_lock;

/*
 * Filesystems get mounted to the VFS as drvX:, where X is an increasing number.
 * This value stores the number the next disk gets.
 */
static int next_mounted_disk_num = 0;

/*
 * Maps a drive type to a string that will form part of the drive name.
 */
static char* type_strings[__DISKUTIL_NUM_TYPES] = {
	[DISKUTIL_TYPE_FIXED] 	    = "hd",
	[DISKUTIL_TYPE_FLOPPY] 	    = "fd",
	[DISKUTIL_TYPE_NETWORK]		= "net",
	[DISKUTIL_TYPE_OPTICAL]		= "cd",
	[DISKUTIL_TYPE_OTHER]		= "other",
	[DISKUTIL_TYPE_RAM]			= "ram",
	[DISKUTIL_TYPE_REMOVABLE]	= "rm",
	[DISKUTIL_TYPE_VIRTUAL]		= "virt",
};

/**
 * Initialises the disk utility functions. Must be called before any partitions are created
 * or any drive names are generated.
 * 
 * @maxirql IRQL_STANDARD 
 */
void InitDiskUtil(void) {
    EXACT_IRQL(IRQL_STANDARD);
    InitSpinlock(&type_table_lock, "diskutil", IRQL_SCHEDULER);
    memset(type_table, 0, sizeof(type_table));
}

/**
 * Given a string and an integer less than 1000, it converts the integer to a string,
 * and appends it to the end of the existing string, in place. The string should have enough 
 * buffer allocated to fit the number.
 * 
 * @param str The string to append the number to
 * @param num The number to append
 * @return 0 on success, or EINVAL if `num` is >= 1000, or `str is NULL. 
 * 
 * @maxirql IRQL_HIGH
 */
static int AppendNumberToString(char* str, int num) {
    MAX_IRQL(IRQL_HIGH);

    if (str == NULL || num >= 1000) {
        return EINVAL;
    }

    char num_str[4];
    memset(num_str, 0, 4);
    if (num < 10) {
        num_str[0] = num + '0';

    } else if (num < 100) {
        num_str[0] = (num / 10) + '0';
        num_str[1] = (num % 10) + '0';

    } else {
        num_str[0] = (num / 100) + '0';
        num_str[1] = ((num / 10) % 10) + '0';
        num_str[2] = (num % 10) + '0';
    }

    strcat(str, num_str);
    return 0;
}

/**
 * Returns the name the next-mounted filesystem should receive (e.g. drv0, drv1).
 * Each call to this function will return a different string. The caller is responsible for
 * freeing the returned string.
 * 
 * @return A caller-free string representing the drive name.
 * 
 * @maxirql IRQL_SCHEDULER
 */
char* GenerateNewMountedDiskName() {
    MAX_IRQL(IRQL_SCHEDULER);

    char name[16];
    strcpy(name, "drv");

    AcquireSpinlockIrql(&type_table_lock);
    int disk_num = next_mounted_disk_num++;
    ReleaseSpinlockIrql(&type_table_lock);

    AppendNumberToString(name, disk_num);
    return strdup(name);
}

/**
 * Returns the name the next-mounted raw disk should receive, based on its type (e.g. raw-hd0, raw-hd1, 
 * raw-fd0). Each call to this function will return a different string. The caller is responsible for 
 * freeing the returned string.
 * 
 * @param type The type of disk (one of DISKUTIL_TYPE_...)
 * @return The caller-free string representing the drive name.
 * 
 * @maxirql IRQL_SCHEDULER
 */
char* GenerateNewRawDiskName(int type) {
    MAX_IRQL(IRQL_SCHEDULER);

    char name[16];
    strcpy(name, "raw-");

    if (type >= __DISKUTIL_NUM_TYPES || type < 0) {
        type = DISKUTIL_TYPE_OTHER;
    }

    strcat(name, type_strings[type]);

    AcquireSpinlockIrql(&type_table_lock);
    int disk_num = type_table[type]++;
    ReleaseSpinlockIrql(&type_table_lock);

    AppendNumberToString(name, disk_num);
    return strdup(name);
}

/**
 * Generates and returns the name of a partition from its partition index within a drive (e.g. part0, part1)
 * The caller is responsible for freeing the returned string.
 */
static char* GetPartitionNameString(int index) {
    char name[16];
    strcpy(name, "part");
    AppendNumberToString(name, index);
    return strdup(name);
}

/**
 * Given a disk, this function detects, creates and mounts partitions on that disk.
 * For each detected partition, the filesystem is also detected, and that is mounted if it exists.
 * If the disk has no partitions, a 'whole disk partition' will be created, the filesystem will
 * still be detected. This should only be called once per disk, after it has been initialised.
 * 
 * @param disk The disk to scan for partitions
 * 
 * @maxirql IRQL_STANDARD
 */
void CreateDiskPartitions(struct open_file* disk) {
    EXACT_IRQL(IRQL_STANDARD);

    struct open_file** partitions = GetPartitionsForDisk(disk);

    if (partitions == NULL || partitions[0] == NULL) {
        off_t disk_size;
        int res = GetFileSize(disk, &disk_size);
        if (res != 0) {
            return;
        }
        struct vnode* whole_disk_partition = CreatePartition(disk, 0, disk_size, -1)->node;
        VnodeOpCreate(disk->node, &whole_disk_partition, GetPartitionNameString(0), 0, 0);
        return;
    }

    for (int i = 0; partitions[i]; ++i) {
        struct vnode* partition = partitions[i]->node;
        char* str = GetPartitionNameString(i);
        VnodeOpCreate(disk->node, &partition, str, 0, 0);
    }
}

void InitDiskPartitionHelper(struct disk_partition_helper* helper) {
    helper->num_partitions = 0;
}

int DiskFollowHelper(struct disk_partition_helper* helper, struct vnode** out, const char* name) {
    for (int i = 0; i < helper->num_partitions; ++i) {
        if (!strcmp(helper->partition_names[i], name)) {
            *out = helper->partitions[i];
            return 0;
        }
    }

    *out = NULL;
    return EINVAL;
}

int DiskCreateHelper(struct disk_partition_helper* helper, struct vnode** in, const char* name) {
    if (helper->num_partitions >= MAX_PARTITIONS_PER_DISK) {
        return EINVAL;
    }

    helper->partitions[helper->num_partitions] = *in;
    helper->partition_names[helper->num_partitions] = (char*) name;
    helper->num_partitions++;
    return 0;
}