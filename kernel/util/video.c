
#include <video.h>
#include <message.h>

struct msgbox* video_mbox;

void DbgScreenPutchar(char c) {
    if (video_mbox != NULL) {
        struct video_msg msg = (struct video_msg) {
            .type = VIDMSG_PUTCHAR,
            .putchar = {.fg = 0x7, .bg = 0x0, .c = c},
        };
        SendMessage(video_mbox, &msg);
    }    
}

void InitVideoConsole(struct msgbox* mbox) {
    video_mbox = mbox;
}
