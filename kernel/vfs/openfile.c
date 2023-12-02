#include <openfile.h>
#include <spinlock.h>
#include <assert.h>
#include <heap.h>
#include <irql.h>

void ReferenceOpenFile(struct open_file* file) {
	assert(file != NULL);

    AcquireSpinlockIrql(&file->reference_count_lock);
    file->reference_count++;
    ReleaseSpinlockIrql(&file->reference_count_lock);
}

void DereferenceOpenFile(struct open_file* file) {
    assert(file != NULL);

	AcquireSpinlockIrql(&file->reference_count_lock);

    assert(file->reference_count > 0);
    file->reference_count--;

    if (file->reference_count == 0) {
        /*
        * Must release the lock before we delete it so we can put interrupts back on
        */
        ReleaseSpinlockIrql(&file->reference_count_lock);
        FreeHeap(file);
        return;
    }

    ReleaseSpinlockIrql(&file->reference_count_lock);
}

struct open_file* CreateOpenFile(struct vnode* node, int mode, int flags, bool can_read, bool can_write) {
	struct open_file* file = AllocHeap(sizeof(struct open_file));
	file->reference_count = 1;
	file->node = node;
	file->can_read = can_read;
	file->can_write = can_write;
	file->initial_mode = mode;
	file->flags = flags;
	file->seek_position = 0;
	InitSpinlock(&file->reference_count_lock, "open file", IRQL_SCHEDULER);
	return file;
}