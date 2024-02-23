#pragma once

#include <stddef.h>
#include <stdint.h>

#define VIDMSG_CLEAR_SCREEN     0
#define VIDMSG_PUTCHAR          1
#define VIDMSG_PUTCHARS         2
#define VIDMSG_SET_CURSOR       3

#define VID_MAX_PUTCHARS_LEN    26

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

        struct {
            int x;
            int y;

        } setcursor;

        struct {
            char cs[VID_MAX_PUTCHARS_LEN];
            uint8_t fg;
            uint8_t bg;
        } putchars;
    };
};

struct msgbox;

void InitVideoConsole(struct msgbox* mbox);
void SendVideoMessage(struct video_msg msg);
