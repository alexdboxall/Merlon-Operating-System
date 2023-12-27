#include <heap.h>
#include <assert.h>
#include <string.h>
#include <virtual.h>
#include <errno.h>
#include <log.h>
#include <common.h>
#include <video.h>
#include <driver.h>
#include <irql.h>

//#include "img.h"

/*
* VESA Terminal and Video Driver for x86 Systems
* 
* A driver for the BIOS VBE video modes. Implements an emulated VGA terminal which can have an
* arbitrary number of rows and columns, based on the video mode. It also allows for direct
* access to the framebuffer via the video interface.
*/

int SCREEN_WIDTH;
int SCREEN_HEIGHT;

/*
* Additional data that is stored within the device interface.
*/
struct vesa_data {
    int cursor_x;
    int cursor_y;
    uint32_t fg_colour;
    uint32_t bg_colour;
    uint32_t framebuffer_physical;
    uint8_t* framebuffer_virtual;
    int width;
    int pitch;
    int height;
    int depth_in_bits;
};

static struct vesa_data* data = NULL;

/*
* Moves the cursor position to a newline, handling the case where
* it reaches the bottom of the terminal
*/
static void vesa_newline() {
    data->cursor_x = 0;

    assert(data->cursor_y < SCREEN_HEIGHT);

    if (data->cursor_y == SCREEN_HEIGHT - 1) {
        /*
        * Keep the cursor on the final line, but scroll the terminal
        * back and clear the final line.
        */
        memmove((void*) data->framebuffer_virtual, (void*) (data->framebuffer_virtual + data->pitch * 16), data->pitch * (16 * SCREEN_HEIGHT - 16));
        inline_memset((void*) (data->framebuffer_virtual + data->pitch * (16 * SCREEN_HEIGHT - 16)), 0, data->pitch * 16);

    } else {
        data->cursor_y++;
    }
}

void (*GenericVideoDrawConsoleCharacter)(uint8_t*, int, int, int, int, uint32_t, uint32_t, char) = NULL;
void (*GenericVideoPutpixel)(uint8_t*, int, int, int, int, uint32_t);

void VesaPutpixel(int x, int y, uint32_t colour) {
    GenericVideoPutpixel(data->framebuffer_virtual, data->pitch, data->depth_in_bits, x, y, colour);
}

static void vesa_render_character(char c) {
    GenericVideoDrawConsoleCharacter(data->framebuffer_virtual, data->pitch, data->depth_in_bits, data->cursor_x * 8, data->cursor_y * 16, 0x000000, 0xFFFFFF, c);
}

/*
* Writes a character to the current cursor position and increments the 
* cursor location. Can handle newlines and the edges of the screen.
*/
void DrvConsolePutchar(char c) {
    if (c == '\n') {
        vesa_newline();
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

    vesa_render_character(c);

    data->cursor_x++;
    if (data->cursor_x == SCREEN_WIDTH) {
        vesa_newline();
    }
}

void DrvConsolePuts(char* s) {
    for (int i = 0; s[i]; ++i) {
        DrvConsolePutchar(s[i]);
    }
}

/*
* Set up the hardware, and install the device into the virtual filesystem.
*/
void InitVesa(void) {
    RequireDriver("sys:/cmnvideo.sys");
    GenericVideoPutpixel = (void (*)(uint8_t*, int, int, int, int, uint32_t)) GetSymbolAddress("GenericVideoPutpixel");
    GenericVideoDrawConsoleCharacter = (void (*)(uint8_t*, int, int, int, int, uint32_t, uint32_t, char)) GetSymbolAddress("GenericVideoDrawConsoleCharacter");

    extern uint32_t vesa_framebuffer;
    extern uint16_t vesa_pitch;
    extern uint16_t vesa_height;
    extern uint16_t vesa_width;
    extern uint8_t vesa_depth;

    data = AllocHeap(sizeof(struct vesa_data));
    data->cursor_x = 0;
    data->cursor_y = 0;
    data->fg_colour = 0xC0C0C0;
    data->bg_colour = 0x000000;
    data->pitch = vesa_pitch;
    data->width = vesa_width;
    data->height = vesa_height;
    data->depth_in_bits = vesa_depth;
    data->framebuffer_physical = vesa_framebuffer;
    
    SCREEN_WIDTH = data->width / 8;
    SCREEN_HEIGHT = data->height / 16;

    LogWriteSerial("%d x %d. depth = %d\n", vesa_width, vesa_height, vesa_depth);

    data->framebuffer_virtual = (uint8_t*) MapVirt(data->framebuffer_physical, 0, data->pitch * data->height, VM_LOCK | VM_MAP_HARDWARE | VM_READ | VM_WRITE, NULL, 0);
    LogWriteSerial("Framebuffer is at 0x%X\n", data->framebuffer_virtual);
    
    /*
    * Clear the screen.
    */
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; ++i) {
        DrvConsolePutchar(' ');
    }
    
    data->cursor_x = 0;
    data->cursor_y = 0;

    struct video_driver driver;
    driver.putchar = DrvConsolePutchar;
    driver.puts = DrvConsolePuts;
    InitVideoConsole(driver);

    /*int LINES_PER_SECTION = 50;

    for (int y = 0; y < vesa_height; ++y) {
        for (int x = 0; x < vesa_width; ++x) {
            int hue = 359 * x / vesa_width;
            int sat = (y % LINES_PER_SECTION) * (100 / LINES_PER_SECTION);
            int val = 100 * (y / LINES_PER_SECTION) / (vesa_height / LINES_PER_SECTION);
            uint32_t col = HsvToRgb(hue, sat, val);
            VesaPutpixel(x, y, col);
        }
    }*/

    /*int i = 0;
    for (int y = 0; y < 768; ++y) {
        for (int x = 0; x < 1024; ++x) {
            uint32_t col = test_image[i + 2];
            col <<= 8;
            col |= test_image[i + 1];
            col <<= 8;
            col |= test_image[i];
            i += 4;
            VesaPutpixel(x, y, col);
        }
    }*/

    ((void (*)(uint8_t*, int, int, int, int, int, int, uint32_t)) GetSymbolAddress("GenericVideoPutrect"))
        (data->framebuffer_virtual, data->pitch, data->depth_in_bits, 0, 0, vesa_width, vesa_height, 0x3880F8);

    /*
    for (int y = 0; y < vesa_height; ++y) {
        for (int x = 0; x < vesa_width; ++x) {
            VesaPutpixel(x, y, 0x3880F8);
        }
    }*/
}
