#include <heap.h>
#include <assert.h>
#include <string.h>
#include <virtual.h>
#include <errno.h>
#include <log.h>
#include <common.h>
#include <video.h>
#include <message.h>
#include <thread.h>
#include <panic.h>
#include <machine/portio.h>

#define SCREEN_WIDTH 80
#define SCREEN_HEIGHT 25

struct vga_data {
    int cursor_x;
    int cursor_y;
    uint16_t colour;
};

static LOCKED_DRIVER_DATA struct vga_data data = {
    .cursor_x = 0,
    .cursor_y = 0,
    .colour = 0x0700
};

static void LOCKED_DRIVER_CODE VgaSetCursor(void) {
    int pos = data.cursor_y * 80 + data.cursor_x;
    outb(0x3D4, 0x0E);
    outb(0x3D5, pos >> 8);
    outb(0x3D4, 0x0F);
    outb(0x3D5, pos & 0xFF);
}

static void LOCKED_DRIVER_CODE ClearLine(int y) {
    /*
     * Set the foreground colour of the whole screen, so we can see the cursor.
     */
    uint16_t* mem = ((uint16_t*) (0xC00B8000)) + 80 * y;
    for (int i = 0; i < 80; ++i) {
        *mem++ = data.colour;
    }    
}

/*
 * Moves the cursor position to a newline, handling the case where
 * it reaches the bottom of the terminal
 */
static void LOCKED_DRIVER_CODE VgaNewline(void) {
    data.cursor_x = 0;

    assert(data.cursor_y < SCREEN_HEIGHT);

    if (data.cursor_y == SCREEN_HEIGHT - 1) {
        uint16_t* vgamem = (uint16_t*) (0xC00B8000);
        memmove((void*) vgamem, (void*) (vgamem + 80), 160 * (SCREEN_HEIGHT - 1));
        ClearLine(SCREEN_HEIGHT - 1);

    } else {
        data.cursor_y++;
    }
}

static void LOCKED_DRIVER_CODE VgaRenderCharacter(char c) {
    uint16_t* vgamem = (uint16_t*) (0xC00B8000);
    vgamem += data.cursor_y * 80 + data.cursor_x;
    *vgamem = data.colour | c;
}

void LOCKED_DRIVER_CODE DrvConsolePutchar(char c) {
    if (c == '\n') {
        VgaNewline();
    } else if (c == '\r') {
        data.cursor_x = 0;
    } else if (c == '\b') {
        /*
         * Needs to be able to backspace past the beginning of the line (i.e. 
         * when you write a line of the terminal that goes across multiple 
         * lines, then you delete it).
         */
        if (data.cursor_x > 0) {
            data.cursor_x--;
        } else {
            data.cursor_x = SCREEN_WIDTH - 1;
            data.cursor_y--;
        }
    } else if (c == '\t') {
        DrvConsolePutchar(' ');
        while (data.cursor_x % 8 != 0) {
            DrvConsolePutchar(' ');
        }
    } else {
        VgaRenderCharacter(c);
        data.cursor_x++;
        if (data.cursor_x == SCREEN_WIDTH) {
            VgaNewline();
        }
    }
}

static void LOCKED_DRIVER_CODE PanicHandler(int, const char* msg) {
    const char* str = "\n\nPANIC               \n";
    data.colour = 0x1F00;
    for (int i = 0; str[i]; ++i) {
        DrvConsolePutchar(str[i]);
    }
    for (int i = 0; msg[i]; ++i) {
        DrvConsolePutchar(msg[i]);
    }
}

static void EnableCursor(void) {
    outb(0x3D4, 0x09);
    outb(0x3D5, 15);
    outb(0x3D4, 0x0B);
    outb(0x3D5, 14);
    outb(0x3D4, 0x0A);
    outb(0x3D5, 13);
}

static void ClearScreen(void) {
    data.cursor_x = 0;
    data.cursor_y = 0;
    for (int i = 0; i < SCREEN_HEIGHT; ++i) {
        ClearLine(i);
    }
}

static void MessageLoop(void*) {
    SetGraphicalPanicHandler(PanicHandler);

    struct msgbox* mbox = CreateMessageBox("vga", sizeof(struct video_msg));
    InitVideoConsole(mbox);

    while (true) {
        struct video_msg msg;
        ReceiveMessage(mbox, &msg);

        switch (msg.type) {
        case VIDMSG_CLEAR_SCREEN:
            data.colour = (msg.clear.fg << 8) | (msg.clear.bg << 12);
            ClearScreen();
            break;
        case VIDMSG_PUTCHAR: 
            data.colour = (msg.putchar.fg << 8) | (msg.putchar.bg << 12);
            DrvConsolePutchar(msg.putchar.c);
            VgaSetCursor();
            break;
        case VIDMSG_PUTCHARS: {
            data.colour = (msg.putchars.fg << 8) | (msg.putchar.bg << 12);
            for (int i = 0; msg.putchars.cs[i] && i < VID_MAX_PUTCHARS_LEN; ++i) {
                DrvConsolePutchar(msg.putchars.cs[i]);
            }
            VgaSetCursor();
            break;
        default:
            break;
        }
    }  
}

void InitVga(void) {
    ClearScreen();
    EnableCursor();
    CreateThread(MessageLoop, NULL, GetVas(), "vga");
}
