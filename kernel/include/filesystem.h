#pragma once

#include <common.h>

struct file;

typedef int(*fs_mount_creator)(struct file*, struct file**);

void InitFilesystemTable(void);
int RegisterFilesystem(char* fs_name, fs_mount_creator mount);
int MountFilesystemForDisk(struct file* partition);