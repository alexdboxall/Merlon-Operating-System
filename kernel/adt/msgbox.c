
/*
 * adt/msgbox.c - Message Queues
 */

#include <heap.h>
#include <assert.h>
#include <common.h>
#include <spinlock.h>
#include <string.h>
#include <semaphore.h>
#include <errno.h>
#include <thread.h>
#include <panic.h>
#include <log.h>
#include <irql.h>
#include <transfer.h>
#include <linkedlist.h>

// TODO: this should use linked lists instead of a fixed array

struct msgbox {
    char* name;
    int payload_size;
    struct linked_list* data;
    struct spinlock lock;
    struct semaphore* sem;
};

struct msgbox* CreateMessageBox(const char* name, int payload_size) {
    struct msgbox* mbox = AllocHeap(sizeof(struct msgbox));
    *mbox = (struct msgbox) {
        .name = strdup(name),
        .payload_size = payload_size,
        .data = ListCreate(),
        .sem = CreateSemaphore("msgboxs", SEM_BIG_NUMBER, SEM_BIG_NUMBER),
    };
    InitSpinlock(&mbox->lock, "msgboxl", IRQL_SCHEDULER);
    return mbox;
}

void DestroyMessageBox(struct msgbox* mbox) {
    // TODO: empty out the contents of the list properly
    ListDestroy(mbox->data);
    FreeHeap(mbox->name);
    FreeHeap(mbox);
}

int SendMessage(struct msgbox* mbox, void* payload) {
    /* 
     * Need to copy it into kernel memory as messages may cross VAS boundaries.
     */
    uint8_t* data = AllocHeap(mbox->payload_size);
    memcpy(data, payload, mbox->payload_size);

    AcquireSpinlock(&mbox->lock);
    ListInsertEnd(mbox->data, data);
    ReleaseSpinlock(&mbox->lock);

    /*
     * Allow the message to be received.
     */
    ReleaseSemaphore(mbox->sem);
    return 0;
}

int ReceiveMessage(struct msgbox* mbox, void* payload) {
    int res = AcquireSemaphore(mbox->sem, -1);  // TODO: make this an EINTR-able acquire
    if (res != 0) {
        return res;
    }

    AcquireSpinlock(&mbox->lock);
    void* data = ListGetDataFromNode(ListGetFirstNode(mbox->data));
    ListDeleteIndex(mbox->data, 0);
    ReleaseSpinlock(&mbox->lock);

    memcpy(payload, data, mbox->payload_size);
    FreeHeap(data);

    return 0;
}