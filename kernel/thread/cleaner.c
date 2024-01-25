
#include <thread.h>
#include <virtual.h>
#include <heap.h>
#include <message.h>

struct cleaner_msg {
    struct thread* thr;
};

static struct msgbox* cleaner_mbox;

static void DestroyThread(struct thread* thr) {
    UnmapVirt(thr->kernel_stack_top - thr->kernel_stack_size, thr->kernel_stack_size);
    FreeHeap(thr->name);
    FreeHeap(thr);
}

static void CleanerThread(void*) {
    struct cleaner_msg msg;
    while (true) {
        ReceiveMessage(cleaner_mbox, &msg);
        DestroyThread(msg.thr);
    }
}

void TerminateThread(struct thread* thr) {
    LockScheduler();

    if (thr == GetThread()) {
        SendMessage(cleaner_mbox, &((struct cleaner_msg) {.thr = thr}));
        BlockThread(THREAD_STATE_TERMINATED);
    } else {
        thr->needs_termination = true;
    }

    UnlockScheduler();
}

void InitCleaner(void) {
    cleaner_mbox = CreateMessageBox("cleaner", sizeof(struct cleaner_msg));
    CreateThread(CleanerThread, NULL, GetVas(), "cleaner");
}
