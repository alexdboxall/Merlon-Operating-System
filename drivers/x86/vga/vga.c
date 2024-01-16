#include <heap.h>
#include <assert.h>
#include <string.h>
#include <virtual.h>
#include <errno.h>
#include <log.h>
#include <common.h>
#include <video.h>
#include <machine/portio.h>

#define SCREEN_WIDTH 80
#define SCREEN_HEIGHT 25

struct vga_data {
    int cursor_x;
    int cursor_y;
};

static struct vga_data data = {
    .cursor_x = 0,
    .cursor_y = 0,
};

static void VgaSetCursor(void) {
    int pos = data.cursor_y * 80 + data.cursor_x;
    outb(0x3D4, 0x0E);
    outb(0x3D5, pos >> 8);
    outb(0x3D4, 0x0F);
    outb(0x3D5, pos & 0xFF);
}

static void ClearLine(int y) {
    /*
    * Ensure the foreground colour is to grey so we can see the cursor.
    */
    uint16_t* mem = ((uint16_t*) (0xC00B8000)) + 80 * y;
    for (int i = 0; i < 80; ++i) {
        *mem++ = 0x0700;
    }    
}

/*
* Moves the cursor position to a newline, handling the case where
* it reaches the bottom of the terminal
*/
static void VgaNewline(void) {
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

static void VgaRenderCharacter(char c) {
    uint16_t* vgamem = (uint16_t*) (0xC00B8000);
    vgamem += data.cursor_y * 80 + data.cursor_x;
    *vgamem = 0x0700 | c;
}

void DrvConsolePutchar(char c) {
    if (c == '\n') {
        VgaNewline();
    } else if (c == '\r') {
        data.cursor_x = 0;
    } else if (c == '\b') {
        /*
        * Needs to be able to backspace past the beginning of the line (i.e. 
        * when you write a line of the terminal that goes across multiple lines,
        * then you delete it).
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
    VgaSetCursor();
}

void DrvConsolePuts(char* s) {
    for (int i = 0; s[i]; ++i) {
        DrvConsolePutchar(s[i]);
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

void InitVga(void) {
    for (int i = 0; i < SCREEN_HEIGHT; ++i) {
        ClearLine(i);
    }
    EnableCursor();
    
    struct video_driver driver;
    driver.putchar = DrvConsolePutchar;
    driver.puts = DrvConsolePuts;
    InitVideoConsole(driver);
}
