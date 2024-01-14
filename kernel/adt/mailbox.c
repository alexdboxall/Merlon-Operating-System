
#include <heap.h>
#include <assert.h>
#include <common.h>
#include <spinlock.h>
#include <semaphore.h>
#include <errno.h>
#include <thread.h>
#include <panic.h>
#include <log.h>
#include <irql.h>

struct mailbox {
    uint8_t* data;
    int total_size;
    int used_size;
    int start_pos;
    int end_pos;
    struct semaphore* full_sem;
    struct semaphore* empty_sem;
    struct semaphore* add_mtx;
    struct semaphore* get_mtx;
    struct spinlock inner_mtx;
};

struct mailbox* MailboxCreate(int size) {
    struct mailbox* mbox = AllocHeap(sizeof(struct mailbox));
    *mbox = (struct mailbox) {
        .data = AllocHeap(size),
        .total_size = size,
        .used_size = 0,
        .start_pos = 0,
        .end_pos = 0,
        .full_sem = CreateSemaphore("mbfull", size, 0),
        .empty_sem = CreateSemephore("mbempty", size, size),
        .add_mtx = CreateMutex("mbadd"),
        .get_mtx = CreateMutex("mbget"),
        .inner_mtx = CreateMutex("mbinner")
    };
    return mbox;
}

// Add()            -> with timeout = 0, it acts like TryAdd()
// WaitAddable()    -> with timeout = 0, it acts like IsAddable()

static int MailboxWaitAddableInternal(struct mailbox* mbox, int timeout) {
    int res = AcquireMutex(mbox->add_mtx, timeout);
    if (res != 0) {
        return res;
    }
    res = AcquireSemaphore(mbox->empty_sem, timeout);
    if (res != 0) {
        ReleaseMutex(mbox->add_mtx);
        return res;
    }
    return 0;
}

int MailboxWaitAddable(struct mailbox* mbox, int timeout) {
    int res = MailboxWaitAddableInternal(mbox);
    if (res != 0) {
        return res;
    }
    ReleaseSemaphore(mbox->empty_sem);    // undo what MailboxWaitAddableInternal did
    ReleaseMutex(mbox->add_mtx);
    return 0;
}

int MailboxAdd(struct mailbox* mbox, int timeout, uint8_t data) {
    int res = MailboxWaitAddable(mbox);
    if (res != 0) {
        return res;
    }

    AcquireMutex(mbox->inner_mtx, -1);
    mbox->buffer[mbox->end_pos] = c;
    mbox->end_pos = (mbox->end_pos + 1) % mbox->total_size;
    mbox->used_size++;
    ReleaseMutex(mbox->inner_mtx);
    ReleaseMutex(mbox->add_mtx);
    ReleaseSemaphore(mbox->full_sem);
    return 0;
}

// the "GET" versions are the same except you swap empty_sem <---> full_sem, and use get_sem instead of add_sem
