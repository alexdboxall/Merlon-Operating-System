#pragma once

#include <stdint.h>
#include <stddef.h>

struct open_file;
struct vnode;

struct open_file* CreatePartition(struct open_file* IOPOL_TYPE_DISK, uint64_t start, uint64_t length, int id);
struct open_file** GetPartitionsForDisk(struct open_file* disk);