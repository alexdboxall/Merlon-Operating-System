#include <_dbg_font.h>
#include <heap.h>
#include <assert.h>
#include <string.h>
#include <virtual.h>
#include <errno.h>
#include <log.h>

/*
* VESA Terminal and Video Driver for x86 Systems
* 
* A driver for the BIOS VBE video modes. Implements an emulated VGA terminal which can have an
* arbitrary number of rows and columns, based on the video mode. It also allows for direct
* access to the framebuffer via the video interface.
*/

int SCREEN_WIDTH = 80;//128;
int SCREEN_HEIGHT = 25;//48;

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

/*static struct vesa_data ad = {
    .cursor_x = 0,
    .cursor_y = 0
};
static struct vesa_data* data = &ad;*/
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

        /*uint16_t* vgamem = (uint16_t*) (0xC00B8000);
        memmove((void*) vgamem, (void*) (vgamem + 80), 160 * 24);
        memset((void*) (vgamem + 80 * 24), 0, 160);*/

        memmove((void*) data->framebuffer_virtual, (void*) (data->framebuffer_virtual + data->pitch * 16), data->pitch * (16 * SCREEN_HEIGHT - 16));
        inline_memset((void*) (data->framebuffer_virtual + data->pitch * (16 * SCREEN_HEIGHT - 16)), 0, data->pitch * 16);

    } else {
        data->cursor_y++;
    }
}

static uint32_t ConvertToColourDepth(uint32_t colour, int bits) {
    if (bits > 16) return colour;

    uint8_t r = (colour >> 16) & 0xFF;
    uint8_t g = (colour >> 8) & 0xFF;
    uint8_t b = (colour >> 0) & 0xFF;

    if (bits == 16) {
        uint32_t nr = r >> 3;
        uint32_t ng = g >> 2;
        uint32_t nb = b >> 3;
        return (nr << 11) | (ng << 5) | nb;

    } else if (bits == 15) {
        uint32_t nr = r >> 3;
        uint32_t ng = g >> 3;
        uint32_t nb = b >> 3;
        return (nr << 10) | (ng << 5) | nb;

    } else {
        // TODO: do this better, for now, I'll just map to the 16 greyscale entrise
        int grey = ((r + g + b) / 3) >> 4;
        return 0x10 + grey;
    }
}

static void vesa_render_character(char c) {
    // TODO: this assumes 24bpp

    /*uint16_t* vgamem = (uint16_t*) (0xC00B8000);
    vgamem += data->cursor_y * 80 + data->cursor_x;
    *vgamem = 0x0700 | c;
    return;*/
    
    for (int i = 0; i < 16; ++i) {
        if (data->depth_in_bits == 24) {
            uint8_t* position = data->framebuffer_virtual + (data->cursor_y * 16 + i) * data->pitch + data->cursor_x * 8 * 3;
            uint8_t font_char = terminal_font[((uint8_t) c) * 16 + i];

            for (int j = 0; j < 8; ++j) {
                uint32_t colour = (font_char & 0x80) ? data->fg_colour : data->bg_colour;
                font_char <<= 1;

                *position++ = (colour >> 0) & 0xFF;
                *position++ = (colour >> 8) & 0xFF;
                *position++ = (colour >> 16) & 0xFF;
            }

        } else if (data->depth_in_bits == 32) {
            uint8_t* position = data->framebuffer_virtual + (data->cursor_y * 16 + i) * data->pitch + data->cursor_x * 8 * 4;
            uint8_t font_char = terminal_font[((uint8_t) c) * 16 + i];

            for (int j = 0; j < 8; ++j) {
                uint32_t colour = (font_char & 0x80) ? data->fg_colour : data->bg_colour;
                font_char <<= 1;

                *position++ = (colour >> 0) & 0xFF;
                *position++ = (colour >> 8) & 0xFF;
                *position++ = (colour >> 16) & 0xFF;
                position++;
            }

        } else if (data->depth_in_bits == 15 || data->depth_in_bits == 16) {
            uint8_t* position = data->framebuffer_virtual + (data->cursor_y * 16 + i) * data->pitch + data->cursor_x * 8 * 2;
            uint8_t font_char = terminal_font[((uint8_t) c) * 16 + i];

            for (int j = 0; j < 8; ++j) {
                uint32_t colour = ConvertToColourDepth((font_char & 0x80) ? data->fg_colour : data->bg_colour, data->depth_in_bits);
                font_char <<= 1;

                *position++ = (colour >> 0) & 0xFF;
                *position++ = (colour >> 8) & 0xFF;
            }

        } else {
            uint8_t* position = data->framebuffer_virtual + (data->cursor_y * 16 + i) * data->pitch + data->cursor_x * 8;
            uint8_t font_char = terminal_font[((uint8_t) c) * 16 + i];

            for (int j = 0; j < 8; ++j) {
                uint32_t colour = ConvertToColourDepth((font_char & 0x80) ? data->fg_colour : data->bg_colour, data->depth_in_bits);
                font_char <<= 1;

                *position++ = (colour >> 0) & 0xFF;
            }
        }
    }
    
}

/*
* Writes a character to the current cursor position and increments the 
* cursor location. Can handle newlines and the edges of the screen.
*/
void DbgScreenPutchar(char c) {
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
        DbgScreenPutchar(' ');
        while (data->cursor_x % 8 != 0) {
            DbgScreenPutchar(' ');
        }
        return;
    }

    vesa_render_character(c);

    data->cursor_x++;
    if (data->cursor_x == SCREEN_WIDTH) {
        vesa_newline();
    }
}

void DbgScreenPuts(char* s) {
    for (int i = 0; s[i]; ++i) {
        DbgScreenPutchar(s[i]);
    }
}

/*
* Set up the hardware, and install the device into the virtual filesystem.
*/
void InitDbgScreen(void) {
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

    data->framebuffer_virtual = (uint8_t*) MapVirt(data->framebuffer_physical, 0, data->pitch * data->height, VM_LOCK | VM_MAP_HARDWARE | VM_READ | VM_WRITE, NULL, 0);

    /*
    * Clear the screen.
    */
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; ++i) {
        DbgScreenPutchar(' ');
    }
    
    data->cursor_x = 0;
    data->cursor_y = 0;
}
