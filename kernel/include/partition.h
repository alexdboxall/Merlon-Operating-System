#pragma once

#include <stdint.h>
#include <stddef.h>

struct open_file;
struct vnode;

struct open_file* CreatePartition(struct open_file* disk, uint64_t start, uint64_t length, int id, int sector_size, int media_type, bool boot);
struct open_file** GetPartitionsForDisk(struct open_file* disk);