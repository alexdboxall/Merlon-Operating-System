#pragma once

#include <common.h>
#include <sys/types.h>
#include <spinlock.h>

struct vnode;

struct file {
    bool can_read;
    bool can_write;
    mode_t initial_mode;
    size_t seek_position;
	int flags;
	int reference_count;
    struct spinlock reference_count_lock;
    struct vnode* node;
};

struct file* CreateFile(struct vnode* node, int mode, int flags, bool can_read, bool can_write);
void ReferenceFile(struct file* file);
void DereferenceFile(struct file* file);
