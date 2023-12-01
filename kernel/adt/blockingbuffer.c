
#include <heap.h>
#include <assert.h>
#include <common.h>
#include <spinlock.h>
#include <semaphore.h>
#include <errno.h>
#include <irql.h>

struct blocking_buffer {
    uint8_t* buffer;
    int total_size;
    int used_size;
    int start_pos;
    int end_pos;
    struct semaphore* sem;
    struct spinlock lock;
};

struct blocking_buffer* BlockingBufferCreate(int size) {
    assert(size > 0);

    struct blocking_buffer* buffer = AllocHeap(sizeof(struct blocking_buffer));
    buffer->buffer = AllocHeap(size);
    buffer->total_size = size;
    buffer->used_size = 0;
    buffer->start_pos = 0;
    buffer->end_pos = 0;
    buffer->sem = CreateSemaphore(size, size);
    InitSpinlock(&buffer->lock, "blocking buffer", IRQL_SCHEDULER);
    
    return buffer;
}

void BlockingBufferDestroy(struct blocking_buffer* buffer) {
    FreeHeap(buffer->sem);
    FreeHeap(buffer->buffer);
    FreeHeap(buffer);
}

int BlockingBufferAdd(struct blocking_buffer* buffer, uint8_t c) {
    AcquireSpinlockIrql(&buffer->lock);

    if (buffer->used_size == buffer->total_size) {
        ReleaseSpinlockIrql(&buffer->lock);
        return ENOBUFS;
    }

    buffer->buffer[buffer->end_pos] = c;
    buffer->end_pos = (buffer->end_pos + 1) % buffer->total_size;
    buffer->used_size++;

    ReleaseSpinlockIrql(&buffer->lock);

    /*
     * Wake up someone waiting for a character to enter the buffer - or make it so 
     * next time someone wants a character they can grab it straight away.
     */
    ReleaseSemaphore(buffer->sem);
    return 0;
}

static uint8_t BlockingBufferGetAfterAcquisition(struct blocking_buffer* buffer) {
    AcquireSpinlockIrql(&buffer->lock);

    uint8_t c = buffer->buffer[buffer->start_pos];
    buffer->start_pos = (buffer->start_pos + 1) % buffer->total_size;
    buffer->used_size--;

    ReleaseSpinlockIrql(&buffer->lock);

    return c;
}

uint8_t BlockingBufferGet(struct blocking_buffer* buffer) {
    /*
     * Wait for there to be something to actually read.
     */
    AcquireSemaphore(buffer->sem, -1);
    return BlockingBufferGetAfterAcquisition(buffer);
}

int BlockingBufferTryGet(struct blocking_buffer* buffer, uint8_t* c) {
    assert(c != NULL);

    int result = AcquireSemaphore(buffer->sem, 0);
    if (result == 0) {
        *c = BlockingBufferGetAfterAcquisition(buffer);
        return 0;

    } else {
        return result;
    }
}