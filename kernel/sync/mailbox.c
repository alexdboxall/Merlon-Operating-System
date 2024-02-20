
/*
 * adt/mailbox.c - Blocking Buffers
 *
 * Implements fixed-sized byte queues that can block on read/write if they are
 * empty/full. Useful for implementing pipes and ptys.
 */

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
#include <transfer.h>

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
    struct semaphore* inner_mtx;
};

struct mailbox* MailboxCreate(int size) {
    struct mailbox* mbox = AllocHeap(sizeof(struct mailbox));
    *mbox = (struct mailbox) {
        .data = AllocHeap(size),
        .total_size = size,
        .used_size = 0,
        .start_pos = 0,
        .end_pos = 0,
        .full_sem = CreateSemaphore("mbfull", size, size),
        .empty_sem = CreateSemaphore("mbempty", size, 0),
        .add_mtx = CreateMutex("mbadd"),
        .get_mtx = CreateMutex("mbget"),
        .inner_mtx = CreateMutex("mbinner")
    };
    return mbox;
}

void MailboxDestroy(struct mailbox* mbox) {
    DestroySemaphore(mbox->full_sem, SEM_DONT_CARE);
    DestroySemaphore(mbox->empty_sem, SEM_DONT_CARE);
    DestroyMutex(mbox->add_mtx);
    DestroyMutex(mbox->get_mtx);
    DestroyMutex(mbox->inner_mtx);
    FreeHeap(mbox);
}

static int MailboxWaitAddableInternal(struct mailbox* mbox, int timeout) {
    int res = AcquireMutex(mbox->add_mtx, timeout);
    if (res != 0) {
        return res;
    }
    if ((res = AcquireSemaphore(mbox->empty_sem, timeout))) {
        ReleaseMutex(mbox->add_mtx);
    }
    return res;
}

int MailboxWaitAddable(struct mailbox* mbox, int timeout) {
    int res = MailboxWaitAddableInternal(mbox, timeout);
    if (res != 0) {
        return res;
    }
    ReleaseSemaphore(mbox->empty_sem);
    ReleaseMutex(mbox->add_mtx);
    return 0;
}

int MailboxAddMany(struct mailbox* mbox, int timeout, uint8_t* c, uint64_t max, uint64_t* added) {
    *added = 0;

    if (max <= 0 || added == NULL) {
        return EINVAL;
    }
    
    int res = MailboxWaitAddableInternal(mbox, timeout);
    if (res != 0) {
        return res;
    }

    uint64_t acquisitions = 1;
    while (acquisitions < max && AcquireSemaphore(mbox->empty_sem, 0) == 0) {
        ++acquisitions;
    }

    AcquireMutex(mbox->inner_mtx, -1);
    for (uint64_t i = 0; i < acquisitions; ++i) {
        mbox->data[mbox->end_pos] = c[i];
        mbox->end_pos = (mbox->end_pos + 1) % mbox->total_size;
        mbox->used_size++;
    }
    ReleaseMutex(mbox->inner_mtx);
    ReleaseMutex(mbox->add_mtx);
    
    LockScheduler();
    ReleaseSemaphoreEx(mbox->full_sem, acquisitions);
    UnlockScheduler();

    *added = acquisitions;
    return 0;
}

int MailboxAdd(struct mailbox* mbox, int timeout, uint8_t c) {
    int res = MailboxWaitAddableInternal(mbox, timeout);
    if (res != 0) {
        return res;
    }

    AcquireMutex(mbox->inner_mtx, -1);
    mbox->data[mbox->end_pos] = c;
    mbox->end_pos = (mbox->end_pos + 1) % mbox->total_size;
    mbox->used_size++;
    ReleaseMutex(mbox->inner_mtx);
    ReleaseMutex(mbox->add_mtx);
    ReleaseSemaphore(mbox->full_sem);
    return 0;
}

static int MailboxWaitGettableInternal(struct mailbox* mbox, int timeout) {
    // TODO: you'll want to create a flag in struct semaphore* call 'interruptable'
    //       or have 'timeout == -2' mean non-interruptable
    int res = AcquireMutex(mbox->get_mtx, timeout);
    if (res != 0) {
        return res;
    }
    if ((res = AcquireSemaphore(mbox->full_sem, timeout))) {
        ReleaseMutex(mbox->get_mtx);
    }
    return res;
}

int MailboxWaitGettable(struct mailbox* mbox, int timeout) {
    int res = MailboxWaitGettableInternal(mbox, timeout);
    if (res != 0) {
        return res;
    }
    ReleaseSemaphore(mbox->full_sem);
    ReleaseMutex(mbox->get_mtx);
    return 0;
}

int MailboxGet(struct mailbox* mbox, int timeout, uint8_t* c) {
    int res = MailboxWaitGettableInternal(mbox, timeout);
    if (res != 0) {
        return res;
    }

    AcquireMutex(mbox->inner_mtx, -1);
    
    *c = mbox->data[mbox->start_pos];
    mbox->start_pos = (mbox->start_pos + 1) % mbox->total_size;
    mbox->used_size--;

    ReleaseMutex(mbox->inner_mtx);
    ReleaseMutex(mbox->get_mtx);
    ReleaseSemaphore(mbox->empty_sem);
    return 0;
}

int MailboxGetMany(struct mailbox* mbox, int timeout, uint8_t* c, uint64_t max, uint64_t* added) {
    *added = 0;
    
    if (max <= 0 || added == NULL) {
        return EINVAL;
    }
    
    int res = MailboxWaitGettableInternal(mbox, timeout);
    if (res != 0) {
        return res;
    }

    int acquisitions = 1;
    while (acquisitions < max && AcquireSemaphore(mbox->full_sem, 0) == 0) {
        ++acquisitions;
    }

    AcquireMutex(mbox->inner_mtx, -1);
    for (int i = 0; i < acquisitions; ++i) {
        c[i] = mbox->data[mbox->start_pos];
        mbox->start_pos = (mbox->start_pos + 1) % mbox->total_size;
        mbox->used_size--;
    }
    ReleaseMutex(mbox->inner_mtx);
    ReleaseMutex(mbox->get_mtx);
    
    LockScheduler();
    ReleaseSemaphoreEx(mbox->empty_sem, acquisitions);
    UnlockScheduler();

    *added = acquisitions;
    return 0;
}

int MailboxAccess(struct mailbox* mbox, struct transfer* tr) {
    const int CHUNK_SIZE = 256;

    bool write = tr->direction == TRANSFER_WRITE;
    if (tr->length_remaining == 0) {
        return (write ? MailboxWaitAddable : MailboxWaitGettable)(mbox, 0);
    }

    bool done_any = false;
    while (tr->length_remaining > 0) {
        bool can_block = tr->blockable && !done_any;
        uint8_t c[CHUNK_SIZE];
        uint64_t added;
        int res;
        uint64_t len = MIN(tr->length_remaining, CHUNK_SIZE);
        if (write) {
            uint64_t old_remaining = tr->length_remaining;
            PerformTransfer(c, tr, len);
            uint64_t transferred = old_remaining - tr->length_remaining;
            res = MailboxAddMany(mbox, can_block ? -1 : 0, c, transferred, &added);
            if (transferred != added ) {
                if (added > transferred) {
                    LogWriteSerial("add = %d, transf = %d\n", (int) added, (int) transferred);
                    PanicEx(PANIC_UNKNOWN, "reverting more than we took??");
                }
                RevertTransfer(tr, transferred - added);
                //SleepMilli(10); // give time to fill for hopefully faster transfer
            }
        } else {
            res = MailboxGetMany(mbox, can_block ? -1 : 0, c, len, &added);
            if (added != len) {
                //SleepMilli(10); // give time to fill for hopefully faster transfer
            }
        }
        if (res != 0) {
            return done_any ? 0 : res;
        }
        if (!write) {
            PerformTransfer(c, tr, added);
        }
        done_any = true;
    }
    return 0;
}

