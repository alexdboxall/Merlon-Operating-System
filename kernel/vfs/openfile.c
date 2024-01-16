#include <file.h>
#include <spinlock.h>
#include <assert.h>
#include <heap.h>
#include <irql.h>
#include <vfs.h>
#include <sys/stat.h>
#include <dirent.h>
#include <dev.h>

/**
 * Creates an open file from an open vnode. Open files are file descriptors, and
 * keeps track of seek position, ability to read/write and O_CLOEXEC. Currently,
 * the only flags stored here are O_NONBLOCK and O_APPEND.
 */
struct file* CreateFile(
    struct vnode* node, int mode, int flags, bool can_read, bool can_write
) {
    MAX_IRQL(IRQL_SCHEDULER);
    
	struct file* file = AllocHeap(sizeof(struct file));
	file->reference_count = 1;
	file->node = node;
	file->can_read = can_read;
	file->can_write = can_write;
	file->initial_mode = mode;
	file->flags = flags;
	file->seek_position = 0;
	InitSpinlock(&file->reference_count_lock, "open file", IRQL_SCHEDULER);

    ReferenceVnode(node);
    
	return file;
}

/**
 * Increments the reference counter for an opened file. Must be called everytime
 * a reference to the open file is kept, so its memory can be managed correctly.
 */
void ReferenceFile(struct file* file) {
    MAX_IRQL(IRQL_SCHEDULER);
	assert(file != NULL);

    AcquireSpinlock(&file->reference_count_lock);
    file->reference_count++;
    ReleaseSpinlock(&file->reference_count_lock);
}

/**
 * Decrements the reference counter for an opened file. Should be called when a
 * reference to the open file is removed. If the reference counter reaches zero,
 * the memory behind the open file will be freed. The underlying vnode within 
 * the open file is dereferenced - this counteracts the one done in 
 * CreateFile.
 */
void DereferenceFile(struct file* file) {
    MAX_IRQL(IRQL_SCHEDULER);
    assert(file != NULL);

	AcquireSpinlock(&file->reference_count_lock);

    assert(file->reference_count > 0);
    file->reference_count--;

    if (file->reference_count == 0) {
        ReleaseSpinlock(&file->reference_count_lock);

        if (IFTODT(file->node->stat.st_mode) == DT_FIFO) {
            BreakPipe(file->node);
        }

        DereferenceVnode(file->node);
        FreeHeap(file);
        return;
    }

    ReleaseSpinlock(&file->reference_count_lock);
}
