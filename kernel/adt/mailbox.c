
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

int MailboxRead(struct mailbox* mbox, struct transfer* tr) {  
    if (tr->length_remaining == 0) {
        return MailboxWaitGettable(mbox, 0);
    }

    bool done_any = false;
    while (tr->length_remaining > 0) {
        uint8_t c;
        int res = MailboxGet(mbox, tr->blockable && !done_any ? -1 : 0, &c);
        if (res != 0) {
            return done_any ? 0 : res;
        }
        PerformTransfer(&c, tr, 1);
        done_any = true;
    }

    return 0;
}

int MailboxWrite(struct mailbox* mbox, struct transfer* tr) {  
    if (tr->length_remaining == 0) {
        return MailboxWaitAddableInternal(mbox, 0);
    }

    bool done_any = false;
    while (tr->length_remaining > 0) {
        uint8_t c;
        PerformTransfer(&c, tr, 1);
        int res = MailboxAdd(mbox, tr->blockable && !done_any ? -1 : 0, c);
        if (res != 0) {
            return done_any ? 0 : res;
        }
        done_any = true;
    }

    return 0;
}