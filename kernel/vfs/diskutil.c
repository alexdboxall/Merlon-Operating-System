#include <diskutil.h>
#include <string.h>
#include <irql.h>
#include <assert.h>
#include <spinlock.h>
#include <vfs.h>
#include <errno.h>
#include <partition.h>
#include <sys/stat.h>

static int type_table[__DISKUTIL_NUM_TYPES];
static struct spinlock type_table_lock;
static int next_mounted_disk_num = 0;

void InitDiskUtil(void) {
    InitSpinlock(&type_table_lock, "diskutil", IRQL_SCHEDULER);
    memset(type_table, 0, sizeof(type_table));
}

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

static int AppendNumberToString(char* str, int num) {
    char num_str[4];
    memset(num_str, 0, 4);
    if (num < 10) {
        num_str[0] = num + '0';

    } else if (num < 100) {
        num_str[0] = (num / 10) + '0';
        num_str[1] = (num % 10) + '0';

    } else if (num < 1000) {
        num_str[0] = (num / 100) + '0';
        num_str[1] = ((num / 10) % 10) + '0';
        num_str[2] = (num % 10) + '0';

    } else {
        return EINVAL;
    }

    strcat(str, num_str);
    return 0;
}

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

static char* GetPartitionNameString(int index) {
    char name[16];
    strcpy(name, "part");
    AppendNumberToString(name, index);
    return strdup(name);
}

void CreateDiskPartitions(struct open_file* disk) {
    struct vnode** partitions = GetPartitionsForDisk(disk);

    if (partitions == NULL || partitions[0] == NULL) {
        struct stat st;
        int res = VnodeOpStat(disk->node, &st);
        if (res != 0) {
            return;
        }
        struct vnode* whole_disk_partition = CreatePartition(disk, 0, st.st_size, -1)->node;
        VnodeOpCreate(disk->node, &whole_disk_partition, GetPartitionNameString(0), 0, 0);
        return;
    }

    for (int i = 0; partitions[i]; ++i) {
        struct vnode* partition = partitions[i];
        VnodeOpCreate(disk->node, &partition, GetPartitionNameString(i), 0, 0);
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