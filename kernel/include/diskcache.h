#pragma once

#include <common.h>

#define DISKCACHE_NORMAL    0
#define DISKCACHE_REDUCE    1
#define DISKCACHE_TOSS      2

void InitDiskCaches(void);
void SetDiskCaches(int mode);

struct open_file* CreateDiskCache(struct open_file* underlying_disk);
