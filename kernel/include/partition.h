#pragma once

#include <stdint.h>
#include <stddef.h>

struct file;
struct vnode;

struct file* CreatePartition(struct file* disk, uint64_t start, uint64_t length, int id, int sector_size, int media_type, bool boot);
struct file** GetPartitionsForDisk(struct file* disk);