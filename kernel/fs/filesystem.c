
#include <common.h>
#include <vfs.h>
#include <errno.h>
#include <string.h>
#include <spinlock.h>
#include <irql.h>
#include <diskutil.h>
#include <semaphore.h>
#include <log.h>
#include <filesystem.h>
#include <heap.h>

#include <fs/demofs/demofs.h>


#define MAX_REGISTERED_FILESYSTEMS 8

struct filesystem {
    char* name;
    fs_mount_creator mount_creator;
};

static struct filesystem registered_filesystems[MAX_REGISTERED_FILESYSTEMS];
static int num_filesystems = 0;
static struct semaphore* fs_table_lock;

void InitFilesystemTable(void) {
    num_filesystems = 0;
    fs_table_lock = CreateMutex();

    LogWriteSerial("A\n");
    RegisterFilesystem("demofs", DemofsMountCreator);
    LogWriteSerial("B\n");
}

int RegisterFilesystem(char* fs_name, fs_mount_creator mount) {
    if (fs_name == NULL || mount == NULL) {
        return EINVAL;
    }

    struct filesystem fs;
    fs.name = strdup_pageable(fs_name);
    fs.mount_creator = mount;

    int ret = 0;

    AcquireMutex(fs_table_lock, -1);
    if (num_filesystems < MAX_REGISTERED_FILESYSTEMS) {
        registered_filesystems[num_filesystems++] = fs;
    } else {
        ret = EALREADY;
    }
    registered_filesystems[num_filesystems++] = fs;
    ReleaseMutex(fs_table_lock);

    if (ret != 0) {
        FreeHeap(fs.name);
    }
    
    return ret;
}

int MountFilesystemForDisk(struct open_file* partition) {
    AcquireMutex(fs_table_lock, -1);

    struct open_file* fs = NULL;

    for (int i = 0; i < num_filesystems; ++i) {
        LogWriteSerial("Trying to mount partition as filesystem: %s... ", registered_filesystems[i].name);
        fs = NULL;
        int res = registered_filesystems[i].mount_creator(partition, &fs);
        if (res == 0) {
            LogWriteSerial("success!\n");
            break;
        }
        LogWriteSerial("nope...\n");
    }

    ReleaseMutex(fs_table_lock);

    if (fs == NULL) {
        LogWriteSerial("no fs\n");
        return ENODEV;
    }

    int res = VnodeOpCreate(partition->node, &fs->node, "fs", 0, 0);
    if (res != 0) {
        LogWriteSerial("creating the fs node gave res = %d\n", res);
        return res;
    }

    AddVfsMount(fs->node, GenerateNewMountedDiskName());
    LogWriteSerial("Added mount point...\n");
    return 0;
}