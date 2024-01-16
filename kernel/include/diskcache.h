#pragma once

#include <common.h>

#define DISKCACHE_NORMAL    0
#define DISKCACHE_REDUCE    1
#define DISKCACHE_TOSS      2

void InitDiskCaches(void);
void SetDiskCaches(int mode);

struct file* CreateDiskCache(struct file* underlying_disk);
