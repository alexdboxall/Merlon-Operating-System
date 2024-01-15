#pragma once

#include <common.h>

struct mailbox;


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
    struct semaphore* inner_mtx;
};

struct mailbox* MailboxCreate(int size);
void MailboxDestroy(struct mailbox* mbox);

int MailboxWaitAddable(struct mailbox* mbox, int timeout);
int MailboxAdd(struct mailbox* mbox, int timeout, uint8_t c);
int MailboxWaitGettable(struct mailbox* mbox, int timeout);
int MailboxGet(struct mailbox* mbox, int timeout, uint8_t* c);

int MailboxRead(struct mailbox* mbox, struct transfer* tr);
int MailboxWrite(struct mailbox* mbox, struct transfer* tr);