#include <heap.h>
#include <assert.h>
#include <string.h>
#include <virtual.h>
#include <errno.h>
#include <log.h>
#include <common.h>
#include <video.h>

int LOCKED_DRIVER_DATA SCREEN_WIDTH = 80;
int LOCKED_DRIVER_DATA SCREEN_HEIGHT = 25;

/*
* Additional data that is stored within the device interface.
*/
struct vga_data {
    int cursor_x;
    int cursor_y;
};

static struct vga_data ad = {
    .cursor_x = 0,
    .cursor_y = 0,
};

static LOCKED_DRIVER_DATA struct vga_data* data = &ad;

/*
* Moves the cursor position to a newline, handling the case where
* it reaches the bottom of the terminal
*/
static void LOCKED_DRIVER_CODE VgaNewline() {
    data->cursor_x = 0;

    assert(data->cursor_y < SCREEN_HEIGHT);

    if (data->cursor_y == SCREEN_HEIGHT - 1) {
        /*
        * Keep the cursor on the final line, but scroll the terminal
        * back and clear the final line.
        */

        uint16_t* vgamem = (uint16_t*) (0xC00B8000);
        memmove((void*) vgamem, (void*) (vgamem + 80), 160 * (SCREEN_HEIGHT - 1));
        memset((void*) (vgamem + 80 * (SCREEN_HEIGHT - 1)), 0, 160);
        
    } else {
        data->cursor_y++;
    }
}


static void LOCKED_DRIVER_CODE VgaRenderCharacter(char c) {
    uint16_t* vgamem = (uint16_t*) (0xC00B8000);
    vgamem += data->cursor_y * 80 + data->cursor_x;
    *vgamem = 0x0700 | c;
}

/*
* Writes a character to the current cursor position and increments the 
* cursor location. Can handle newlines and the edges of the screen.
*/
void LOCKED_DRIVER_CODE DrvConsolePutchar(char c) {
    if (c == '\n') {
        VgaNewline();
        return;
    }

    if (c == '\r') {
        data->cursor_x = 0;
        return;
    }

    if (c == '\b') {
        /*
        * Needs to be able to backspace past the beginning of the line (i.e. when
        * you write a line of the terminal that goes across multiple lines, then you
        * delete it).
        */
        if (data->cursor_x > 0) {
            data->cursor_x--;
        } else {
            data->cursor_x = SCREEN_WIDTH - 1;
            data->cursor_y--;
        }
        return;
    }

    if (c == '\t') {
        DrvConsolePutchar(' ');
        while (data->cursor_x % 8 != 0) {
            DrvConsolePutchar(' ');
        }
        return;
    }

    VgaRenderCharacter(c);

    data->cursor_x++;
    if (data->cursor_x == SCREEN_WIDTH) {
        VgaNewline();
    }
}

void LOCKED_DRIVER_CODE DrvConsolePuts(char* s) {
    for (int i = 0; s[i]; ++i) {
        DrvConsolePutchar(s[i]);
    }
}

void InitVga(void) {
    uint16_t* vgamem = (uint16_t*) (0xC00B8000);
    memset(vgamem, 0, 80 * 25 * 2);
    
    struct video_driver driver;
    driver.putchar = DrvConsolePutchar;
    driver.puts = DrvConsolePuts;
    InitVideoConsole(driver);
}
