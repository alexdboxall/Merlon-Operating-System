#pragma once

#include <stddef.h>
#include <stdint.h>

#define VIDMSG_CLEAR_SCREEN     0
#define VIDMSG_PUTCHAR          1

struct video_msg {
    uint8_t type;
    union {
        struct {
            uint8_t fg;
            uint8_t bg;

        } clear;

        struct {
            char c;
            uint8_t fg;
            uint8_t bg;

        } putchar;
    };
};

struct msgbox;

void InitVideoConsole(struct msgbox* mbox);

void SendVideoMessage(struct video_msg msg);
void HoldVideoMessages(void);
void ReleaseVideoMessages(void);
