
#include <heap.h>
#include <stdlib.h>
#include <vfs.h>
#include <log.h>
#include <assert.h>
#include <virtual.h>
#include <errno.h>
#include <transfer.h>
#include <sys/stat.h>
#include <dirent.h>
#include <linkedlist.h>
#include <tree.h>
#include <semaphore.h>
#include <diskcache.h>

static int current_mode = DISKCACHE_NORMAL;
static struct linked_list* cache_list;
static struct semaphore* cache_list_lock = NULL;

struct cache_entry {
    size_t cache_addr;
    size_t num_blocks;
    uint64_t block_num;
};
 
struct cache_data {
    struct file* underlying_disk;
    int block_size;
    struct tree* cache;
    struct semaphore* lock;
};

int Comparator(void* a_, void* b_) {
    struct cache_entry* a = a_;
    struct cache_entry* b = b_;

    if (a->block_num >= b->block_num && a->block_num < b->block_num + b->num_blocks) {
        return 0;
    }
    if (b->block_num >= a->block_num && b->block_num < a->block_num + a->num_blocks) {
        return 0;
    }

    return COMPARE_SIGN(a->block_num, b->block_num);
}

/*static*/ bool IsCacheCreationAllowed(void) { 
    AcquireMutex(cache_list_lock, -1);
    bool retv = current_mode == DISKCACHE_NORMAL;
    ReleaseMutex(cache_list_lock);
    return retv;
}

static struct cache_entry* IsInCache(struct tree* cache, size_t block) {
    struct cache_entry target = (struct cache_entry) {
        .num_blocks = 1, .block_num = block
    };
    return TreeGet(cache, &target);
}

static struct cache_entry* CreateCacheEntry(size_t block_num, size_t blksize, size_t blkcnt) {
    struct cache_entry* entry = AllocHeap(sizeof(struct cache_entry));
    *entry = (struct cache_entry) {
        .block_num = block_num,
        .cache_addr = MapVirt(0, 0, blksize * blkcnt, VM_READ | VM_WRITE | VM_LOCK, NULL, 0),
        .num_blocks = blkcnt
    };
    return entry;
}

static int Read(struct vnode* node, struct transfer* io) {   
    struct cache_data* data = node->data;
    size_t block_size = node->stat.st_blksize;

    assert(((size_t) io->address) % block_size == 0);
    assert(io->length_remaining % block_size == 0);

    uint64_t start_block = ((size_t) io->address) / block_size;
    uint64_t num_blocks = io->length_remaining / block_size;
    
    for (size_t i = 0; i < num_blocks; ++i) {
        size_t current_block = start_block + i;
        struct cache_entry* entry = IsInCache(data->cache, current_block);

        if (entry == NULL) {
            size_t in_a_row = 1;
            while (IsInCache(data->cache, current_block + in_a_row) == NULL) {
                ++in_a_row;
            }

            struct cache_entry* new_entry = CreateCacheEntry(current_block, block_size, in_a_row);
            struct transfer io2 = CreateKernelTransfer(
                (void*) new_entry->cache_addr, 
                in_a_row * block_size, 
                current_block * block_size, 
                TRANSFER_READ
            );

            int res = VnodeOpRead(node, &io2);
            if (res != 0) {
                return res;
            }            

            res = PerformTransfer((void*) new_entry->cache_addr, io, in_a_row * block_size);
            if (res != 0) {
                return res;
            }
            
            if (IsCacheCreationAllowed()) {
                TreeInsert(data->cache, new_entry);
            } else {
                UnmapVirt(new_entry->cache_addr, in_a_row * block_size);
                FreeHeap(new_entry);
            }
            i += in_a_row - 1;

        } else {
            size_t blocks_into_entry = entry->block_num - current_block;
            int res = PerformTransfer(
                (void*) (entry->cache_addr + blocks_into_entry * block_size), 
                io, block_size
            );
            if (res != 0) {
                return res;
            }
        }
    }

    return VnodeOpRead(data->underlying_disk->node, io);
}

static int Write(struct vnode* node, struct transfer* io) {
    struct cache_data* data = node->data;

    // TODO: update the disk cache. On writes, we will *ALWAYS* perform the
    // write operation. this ensures we can actually return a status code back 
    // to the caller. if writes were delayed until e.g. shutdown or a sync, then
    // we would just need to return 0, even if, e.g. the drive was removed!
    return VnodeOpWrite(data->underlying_disk->node, io);
}

static void TossCache(struct cache_data* data) {
    AcquireMutex(data->lock, -1);
    TreeDestroy(data->cache);
    data->cache = TreeCreate();
    ReleaseMutex(data->lock);
}

static void ReduceCache(struct cache_data* data) {
    AcquireMutex(data->lock, -1);
    // .. TODO: do something here...
    ReleaseMutex(data->lock);
}

static int Close(struct vnode* node) {
    TossCache(node->data);
    return 0;
}

static int Create(struct vnode* node, struct vnode** fs, const char* name, int flags, mode_t mode) {
    struct cache_data* data = node->data;
    return VnodeOpCreate(data->underlying_disk->node, fs, name, flags, mode);
}

static int Follow(struct vnode* node, struct vnode** out, const char* name) {
    struct cache_data* data = node->data;
    return VnodeOpFollow(data->underlying_disk->node, out, name);
}

static const struct vnode_operations dev_ops = {
    .read           = Read,
    .write          = Write,
    .close          = Close,
    .create         = Create,
    .follow         = Follow,
};

void RemoveCacheEntryHandler(void* entry_) {
    struct cache_entry* entry = entry_;
    (void) entry;
    LogDeveloperWarning("TODO: we need the block size here...\n");
    LogDeveloperWarning("Not freeing the disk cache...\n");
    //UnmapVirt(entry->cache_addr, entry->num_blocks);
}

struct file* CreateDiskCache(struct file* underlying_disk)
{
    if (VnodeOpDirentType(underlying_disk->node) != DT_BLK) {
        return underlying_disk;
    }

    (void) dev_ops;

    /* @@@ TODO: fix diskcache! */
    return underlying_disk;

#if 0
    // TODO: the disk cache needs a way of synchronising underlying_disk->stat
    // either: allocate it dynamically and just point to the other one, or after
    // each operation on the disk cache, we copy across the stats

    struct vnode* node = CreateVnode(dev_ops, underlying_disk->node->stat);
    struct cache_data* data = AllocHeap(sizeof(struct cache_data));
    data->underlying_disk = underlying_disk;
    data->cache = TreeCreate();
    data->lock = CreateMutex("vcache");
    data->block_size = MAX(ARCH_PAGE_SIZE, underlying_disk->node->stat.st_blksize);
    TreeSetComparator(data->cache, Comparator); 
    TreeSetDeletionHandler(data->cache, RemoveCacheEntryHandler);
    node->data = data;

    struct file* cache = CreateFile(
        node, underlying_disk->initial_mode, underlying_disk->flags, 
        underlying_disk->can_read, underlying_disk->can_write
    );

    AcquireMutex(cache_list_lock, -1);
    ListInsertEnd(cache_list, cache);
    ReleaseMutex(cache_list_lock);

    return cache;
#endif
}

static void ReduceCacheAmounts(bool toss) {
    struct linked_list_node* node = ListGetFirstNode(cache_list);
    while (node != NULL) {
        struct cache_data* data = ((struct file*) ListGetDataFromNode(node))->node->data;
        (toss ? TossCache : ReduceCache)(data);
        node = ListGetNextNode(node);
    }
}

void SetDiskCaches(int mode) {
    /*
     * The PMM calls on allocation / free this before InitDiskCaches is called,
     * so need to guard here.
     */
    if (cache_list_lock == NULL) {
        return;
    }

    /*
     * Prevent a lot of mutex acquisition and releasing that is unnecessary.
     */
    if (mode == current_mode) {
        return;
    }

    /*
     * It's okay to not acquire it - e.g. the PMM may call SetDiskCaches while
     * this code is running anyway, so this avoids a deadlock. This function
     * gets called a lot, so it will just change the cache mode a little later
     * than expected.
     */
    int res = AcquireMutex(cache_list_lock, 0);
    if (res == 0) {
        if (mode == DISKCACHE_REDUCE && current_mode == DISKCACHE_NORMAL) {
            ReduceCacheAmounts(false);

        } else if (mode == DISKCACHE_TOSS && current_mode != DISKCACHE_TOSS) {
            ReduceCacheAmounts(true);
        }

        current_mode = mode;
        ReleaseMutex(cache_list_lock);
    }
}

void InitDiskCaches(void) {
    cache_list = ListCreate();
    cache_list_lock = CreateMutex("vclist");
}