
#include <video.h>
#include <message.h>
#include <spinlock.h>
#include <irql.h>

static struct msgbox* video_mbox;
static struct msgbox* buffer_mbox;
static int hold_count = 0;
static struct spinlock lock;

void InitVideoConsole(struct msgbox* mbox) {
    video_mbox = mbox;
    buffer_mbox = CreateMessageBox("vdbf", sizeof(struct video_msg));
    InitSpinlock(&lock, "video", IRQL_SCHEDULER);
}

void SendVideoMessage(struct video_msg msg) {
    if (buffer_mbox != NULL) {
        AcquireSpinlock(&lock);
        SendMessage(hold_count == 0 ? video_mbox : buffer_mbox, &msg);
        ReleaseSpinlock(&lock);
    }
}

void HoldVideoMessages(void) {
    AcquireSpinlock(&lock);
    ++hold_count;
    ReleaseSpinlock(&lock);
}

void ReleaseVideoMessages(void) {
    AcquireSpinlock(&lock);
    --hold_count;
    if (hold_count == 0) {
        TransferMessages(video_mbox, buffer_mbox);
    }
    ReleaseSpinlock(&lock);
}

void DbgScreenPutchar(char c) {
    SendVideoMessage((struct video_msg) {
        .type = VIDMSG_PUTCHAR,
        .putchar = {.fg = 0x7, .bg = 0x0, .c = c},
    });
}