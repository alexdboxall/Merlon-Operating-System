
#include <video.h>
#include <message.h>
#include <spinlock.h>
#include <irql.h>

static struct msgbox* video_mbox;
static struct spinlock lock;

void InitVideoConsole(struct msgbox* mbox) {
    video_mbox = mbox;
    InitSpinlock(&lock, "video", IRQL_SCHEDULER);
}

void SendVideoMessage(struct video_msg msg) {
    if (video_mbox != NULL) {
        AcquireSpinlock(&lock);
        SendMessage(video_mbox, &msg);
        ReleaseSpinlock(&lock);
    }
}

void DbgScreenPutchar(char c) {
    SendVideoMessage((struct video_msg) {
        .type = VIDMSG_PUTCHAR,
        .putchar = {.fg = 0x7, .bg = 0x0, .c = c},
    });
}