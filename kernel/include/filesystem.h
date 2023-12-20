#pragma once

#include <common.h>

struct open_file;

typedef int(*fs_mount_creator)(struct open_file*, struct open_file**);

void InitFilesystemTable(void);
int RegisterFilesystem(char* fs_name, fs_mount_creator mount);
int MountFilesystemForDisk(struct open_file* partition);