#pragma once

#include <stddef.h>
#include <stdint.h>

#define VIDMSG_CLEAR_SCREEN     0
#define VIDMSG_PUTCHAR          1

struct video_msg {
    uint8_t type;
    union {
        struct {
            char c;
            uint8_t fg;
            uint8_t bg;

        } putchar;
    };
};

struct msgbox;
extern struct msgbox* video_mbox;

void InitVideoConsole(struct msgbox* mbox);
