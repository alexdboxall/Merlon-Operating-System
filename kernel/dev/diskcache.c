
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
#include <avl.h>
#include <semaphore.h>
#include <diskcache.h>

static int current_mode = DISKCACHE_NORMAL;
static struct linked_list* cache_list;
static struct semaphore* cache_list_lock = NULL;

struct cache_entry {
    size_t cache_addr;
    size_t size;
    uint64_t disk_offset;
};
 
struct cache_data {
    struct open_file* underlying_disk;
    int block_size;
    struct avl_tree* cache;
    struct semaphore* lock;
};

/*static*/ bool IsCacheCreationAllowed(void) { 
    AcquireMutex(cache_list_lock, -1);
    bool retv = current_mode == DISKCACHE_NORMAL;
    ReleaseMutex(cache_list_lock);
    return retv;
}

static void SynchroniseEntry(struct cache_entry* entry) {
    // TODO: write to disk...
    (void) entry;
}

static int Read(struct vnode* node, struct transfer* io) {   
    struct cache_data* data = node->data;
    
    // TODO: look in the cache for it! remembering that the transfer size
    //       can be large, and so it may reside in multiple caches, and may
    //       have uncached sections in between!
    
    return VnodeOpRead(data->underlying_disk->node, io);
}

static int Write(struct vnode* node, struct transfer* io) {
    struct cache_data* data = node->data;
    return VnodeOpWrite(data->underlying_disk->node, io);
}

static int Stat(struct vnode* node, struct stat* st) {
    struct cache_data* data = node->data;
    return VnodeOpStat(data->underlying_disk->node, st);
}

static void TossCache(struct cache_data* data) {
    AcquireMutex(data->lock, -1);
    AvlTreeDestroy(data->cache);
    data->cache = AvlTreeCreate();
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

static bool IsSeekable(struct vnode*) {
    return true;
}

static const struct vnode_operations dev_ops = {
    .read           = Read,
    .write          = Write,
    .stat           = Stat,
    .close          = Close,
    .create         = Create,
    .follow         = Follow,
    .is_seekable    = IsSeekable
};

void RemoveCacheEntryHandler(void* entry_) {
    struct cache_entry* entry = entry_;
    SynchroniseEntry(entry);
    UnmapVirt(entry->cache_addr, entry->size);
}

static bool IsACacheableDisk(struct vnode* node) {
    return VnodeOpDirentType(node) == DT_BLK && VnodeOpIsSeekable(node);
}

struct open_file* CreateDiskCache(struct open_file* underlying_disk)
{
    if (!IsACacheableDisk(underlying_disk->node)) {
        /*
         * Just 'pass-through' without the disk cache layer.
         */
        return underlying_disk;
    }

    struct vnode* node = CreateVnode(dev_ops);
    struct cache_data* data = AllocHeap(sizeof(struct cache_data));
    data->underlying_disk = underlying_disk;
    data->cache = AvlTreeCreate();
    data->lock = CreateMutex("vcache");
    data->block_size = 4096;        // TODO: max of block size and ARCH_PAGE_SIZE
    AvlTreeSetDeletionHandler(data->cache, RemoveCacheEntryHandler);
    node->data = data;

    struct open_file* cache = CreateOpenFile(node, underlying_disk->initial_mode, underlying_disk->flags, underlying_disk->can_read, underlying_disk->can_write);

    AcquireMutex(cache_list_lock, -1);
    LinkedListInsertEnd(cache_list, cache);
    ReleaseMutex(cache_list_lock);

    return cache;
}

static void ReduceCacheAmounts(bool toss) {
    struct linked_list_node* node = LinkedListGetFirstNode(cache_list);
    while (node != NULL) {
        struct cache_data* data = ((struct open_file*) LinkedListGetDataFromNode(node))->node->data;
        (toss ? TossCache : ReduceCache)(data);
        node = LinkedListGetNextNode(node);
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

    AcquireMutex(cache_list_lock, -1);

    if (mode == DISKCACHE_REDUCE && current_mode == DISKCACHE_NORMAL) {
        ReduceCacheAmounts(false);

    } else if (mode == DISKCACHE_TOSS && current_mode != DISKCACHE_TOSS) {
        ReduceCacheAmounts(true);
    }

    current_mode = mode;
    ReleaseMutex(cache_list_lock);
}

void InitDiskCaches(void) {
    cache_list = LinkedListCreate();
    cache_list_lock = CreateMutex("vclist");
}