#include <openfile.h>
#include <spinlock.h>
#include <assert.h>
#include <heap.h>
#include <irql.h>
#include <vfs.h>

/**
 * Creates a new open file from an opened vnode. An open file is used to link a vnode with corresponding data,
 * about a particular instance of opening a file: such as a seek position, and ability to read or write; and is
 * used to maintain file descriptor tables for the C userspace library.
 * 
 * Open files maintain a reference counter that can be incremented and decremented with ReferenceOpenFile and
 * DereferenceOpenFile. A newly created open file has a reference count of 1. When the count reaches 0, the memory
 * is freed.
 * 
 * @param node      The already-open vnode to wrap with additional data
 * @param mode      The Unix-permissions passed to the OpenFile when the vnode was opened. Ignored by the kernel
 *                      so far.
 * @param flag      The flags that were passed to OpenFile when the vnode was opened. Should be a bitfield consisting of 
 *                      zero or more of: O_RDONLY, O_WRONLY, O_TRUNC, O_CREAT. See OpenFile for details. The storage of these
 *                      flags in an open file should only be of interest to usermode programs - the flags here may be be zero  
 *                      if the open file was created within the kernel, so the flags should be ignored within the kernel.
 * @param can_read  Whether or not this open file is allowed to make read operations on the underlying vnode
 * @param can_write Whether or not this open file is allowed to make write operations on the underlying vnode
 * 
 * @return A pointer to the newly created open file.
 */
struct open_file* CreateOpenFile(struct vnode* node, int mode, int flags, bool can_read, bool can_write) {
    MAX_IRQL(IRQL_SCHEDULER);
    
	struct open_file* file = AllocHeap(sizeof(struct open_file));
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
 * Increments the reference counter for an opened file. This should be called everytime a reference to
 * the open file is kept, so that its memory can be managed correctly.
 * 
 * @param file The open file to reference
 */
void ReferenceOpenFile(struct open_file* file) {
    MAX_IRQL(IRQL_SCHEDULER);
	assert(file != NULL);

    AcquireSpinlockIrql(&file->reference_count_lock);
    file->reference_count++;
    ReleaseSpinlockIrql(&file->reference_count_lock);
}

/**
 * Decrements the reference counter for an opened file. Should be called whenever a reference to the open
 * file is removed. If the reference counter reaches zero, the memory behind the open file will be freed.
 * The underlying vnode within the open file is not dereferenced - this should be done prior to calling this
 * function.
 * 
 * @param file The open file to dereference
 */
void DereferenceOpenFile(struct open_file* file) {
    MAX_IRQL(IRQL_SCHEDULER);
    assert(file != NULL);

	AcquireSpinlockIrql(&file->reference_count_lock);

    assert(file->reference_count > 0);
    file->reference_count--;

    if (file->reference_count == 0) {
        /*
        * Must release the lock before we delete it so we can put interrupts back on
        */
        ReleaseSpinlockIrql(&file->reference_count_lock);
        DereferenceVnode(file->node);
        FreeHeap(file);
        return;
    }

    ReleaseSpinlockIrql(&file->reference_count_lock);
}
