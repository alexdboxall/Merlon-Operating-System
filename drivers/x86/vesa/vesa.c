#include <heap.h>
#include <assert.h>
#include <string.h>
#include <virtual.h>
#include <errno.h>
#include <log.h>
#include <vfs.h>
#include <panic.h>
#include <voidptr.h>
#include <thread.h>
#include <physical.h>
#include <common.h>
#include <semaphore.h>
#include <swapfile.h>
#include <video.h>
#include <driver.h>
#include <fcntl.h>
#include <irql.h>

#define BGCOL 0x00BBCC //0x00AAAA

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

static struct LOCKED_DRIVER_DATA semaphore* data_lock = NULL;

/*
 * Needs to be locked so the panic handler will work. 
 */
static struct LOCKED_DRIVER_DATA vesa_data* data = NULL;

void (*_GenericVideoDrawConsoleCharacter)(uint8_t*, int, int, int, int, uint32_t, uint32_t, char) = NULL;
void (*_GenericVideoPutpixel)(uint8_t*, int, int, int, int, uint32_t);
void (*_GenericVideoPutrect)(uint8_t*, int, int, int, int, int, int, uint32_t);

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
        memmove(
            (void*) AddVoidPtr(data->framebuffer_virtual, (128 + 16) * data->pitch), 
            (void*) AddVoidPtr(data->framebuffer_virtual, (128 + 16 + 16) * data->pitch), 
            data->pitch * (16 * SCREEN_HEIGHT - 16)
        );
        _GenericVideoPutrect(data->framebuffer_virtual, data->pitch, data->depth_in_bits, 
            64 + 16, 
            (128 + SCREEN_HEIGHT * 16), 
            SCREEN_WIDTH * 8, 
            16, 
            0x0000000
        );

    } else {
        data->cursor_y++;
    }
}

void VesaPutpixel(int x, int y, uint32_t colour) {
    _GenericVideoPutpixel(data->framebuffer_virtual, data->pitch, data->depth_in_bits, x, y, colour);
}

static void vesa_render_character(char c) {
    _GenericVideoDrawConsoleCharacter(
        data->framebuffer_virtual, 
        data->pitch, 
        data->depth_in_bits, 
        data->cursor_x * 8 + 64 + 16, 
        data->cursor_y * 16 + 128 + 16, 
        0x000000, 
        0xFFFFFF, 
        c
    );
}

/*
* Writes a character to the current cursor position and increments the 
* cursor location. Can handle newlines and the edges of the screen.
*/
void DrvConsolePutchar(char c) {
    AcquireMutex(data_lock, -1);

    if (c == '\n') {
        vesa_newline();
        ReleaseMutex(data_lock);
        return;
    }

    if (c == '\r') {
        data->cursor_x = 0;
        ReleaseMutex(data_lock);
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
        ReleaseMutex(data_lock);
        return;
    }

    if (c == '\t') {
        ReleaseMutex(data_lock);
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

    ReleaseMutex(data_lock);
}

void DrvConsolePuts(char* s) {
    for (int i = 0; s[i]; ++i) {
        DrvConsolePutchar(s[i]);
    }
}

void AppendNumberToStringX(char* str, int num) {
    char num_str[8];
    memset(num_str, 0, 8);
    if (num < 10) {
        num_str[0] = num + '0';

    } else if (num < 100) {
        num_str[0] = (num / 10) + '0';
        num_str[1] = (num % 10) + '0';

    } else if (num < 1000) {
        num_str[0] = (num / 100) + '0';
        num_str[1] = ((num / 10) % 10) + '0';
        num_str[2] = (num % 10) + '0';
    
    } else if (num < 10000) {
        num_str[0] = (num / 1000) + '0';
        num_str[1] = ((num / 100) % 10) + '0';
        num_str[2] = ((num / 10) % 10) + '0';
        num_str[3] = (num % 10) + '0';
    
    } else if (num < 100000) {
        num_str[0] = (num / 10000) + '0';
        num_str[1] = ((num / 1000) % 10) + '0';
        num_str[2] = ((num / 100) % 10) + '0';
        num_str[3] = ((num / 10) % 10) + '0';
        num_str[4] = (num % 10) + '0';

    } else {
        strcat(str, "99999+");
        return;
    }

    strcat(str, num_str);
}

void ShowRAMUsage(void*) {
    while (true) {
        SleepMilli(250);

        size_t free = GetFreePhysKilobytes();
        size_t total = GetTotalPhysKilobytes();
        size_t pages_on_swap = GetSwapCount();

        char buffer[128];
        memset(buffer, 0, 128);

        AppendNumberToStringX(buffer, total - free);
        strcat(buffer, " / ");
        AppendNumberToStringX(buffer, total);
        strcat(buffer, " KB used (");
        AppendNumberToStringX(buffer, 100 * free / total);
        strcat(buffer, "% free). ");
        AppendNumberToStringX(buffer, pages_on_swap * 4);
        strcat(buffer, " KB on swapfile.         ");

    
        for (int i = 0; buffer[i]; ++i) {
            AcquireMutex(data_lock, -1);
            _GenericVideoDrawConsoleCharacter(
                data->framebuffer_virtual, 
                data->pitch, 
                data->depth_in_bits, 
                64 + i * 8, 
                64, 
                BGCOL, 
                0x000000, 
                buffer[i]
            );
            ReleaseMutex(data_lock);
        }   
    }
}

static void LOCKED_DRIVER_CODE PanicPutpixel(int x, int y, uint32_t colour) {
    uint8_t* position = data->framebuffer_virtual + y * data->pitch;
    int depth = data->depth_in_bits;
    if (depth == 24) {
        position += x * 3;
        *position++ = (colour >> 0) & 0xFF;
        *position++ = (colour >> 8) & 0xFF;
        *position = (colour >> 16) & 0xFF;

    } else if (depth == 32) {
        position += x * 4;
        *position++ = (colour >> 0) & 0xFF;
        *position++ = (colour >> 8) & 0xFF;
        *position = (colour >> 16) & 0xFF;

    } else if (depth == 15 || depth == 16) {
        position += x * 2;
        *position++ = (colour >> 0) & 0xFF;
        *position = (colour >> 8) & 0xFF;

    } else {
        position += x;
        *position = colour & 0xFF;
    }
}

// 0-9 = digits; 10 = underscore; 11+ = uppercase 
static uint8_t LOCKED_DRIVER_DATA panic_font[16 * (26 + 10 + 1)];

static void LOCKED_DRIVER_CODE PanicDrawCharacter(int pixel_x, int pixel_y, uint32_t adjusted_colour, char c) {
    if (c == ' ') {
        return;
    }
    if (c >= 'a' && c <= 'z') {
        c -= 32;
    }
    
    int char_index;
    if (c >= 'A' && c <= 'Z') {
        char_index = 11 + c - 'A';
    } else if (c >= '0' && c <= '9') {
        char_index = c - '0';
    } else {
        char_index = 10;
    }

    for (int i = 0; i < 16; ++i) {
        uint8_t font_char = panic_font[char_index * 16 + i];

        for (int j = 0; j < 8; ++j) {
            if (font_char & 0x80) {
                PanicPutpixel(pixel_x + j, pixel_y + i, adjusted_colour);
            }
            font_char <<= 1;
        }
    }
}

static void LOCKED_DRIVER_CODE PanicHandler(int code, const char* message) {
    (void) code;
    (void) message;

    uint32_t fg_col = 0xFFFFFF;
    uint32_t bg_col = 0x0000AA;

    if (data->depth_in_bits == 8) {
        bg_col = 0x1;
        fg_col = 0xF;

    } else if (data->depth_in_bits == 15 || data->depth_in_bits == 16) {
        bg_col = 0x15;
        fg_col = data->depth_in_bits == 15 ? 0x7FFF : 0xFFFF;
    }

    for (int y = 0; y < data->height; ++y) {
        for (int x = 0; x < data->width; ++x) {
            PanicPutpixel(x, y, bg_col);
        }
    }

    const char* panic_msg = "KERNEL PANIC";
    for (int i = 0; panic_msg[i]; ++i) {
        PanicDrawCharacter(32 + i * 8, 32, fg_col, panic_msg[i]);
    }

    int code_x = 32 + 3 * 8;
    for (int i = 0; i < 4; ++i) {
        PanicDrawCharacter(code_x, 64, fg_col, (code % 10) + '0');
        code /= 10;
        code_x -= 8;
    }

    for (int i = 0; message[i]; ++i) {
        PanicDrawCharacter(32 + i * 8, 64 + 16, fg_col, message[i]);
    }
}

/*
* Set up the hardware, and install the device into the virtual filesystem.
*/
void InitVesa(void) {
    data_lock = CreateMutex("vesa");
    
    RequireDriver("sys:/cmnvideo.sys");
    _GenericVideoPutpixel = (void (*)(uint8_t*, int, int, int, int, uint32_t)) GetSymbolAddress("GenericVideoPutpixel");
    _GenericVideoDrawConsoleCharacter = (void (*)(uint8_t*, int, int, int, int, uint32_t, uint32_t, char)) GetSymbolAddress("GenericVideoDrawConsoleCharacter");
    _GenericVideoPutrect = (void (*)(uint8_t*, int, int, int, int, int, int, uint32_t)) GetSymbolAddress("GenericVideoPutrect");

    uint8_t* code_page_437 = (uint8_t*) GetSymbolAddress("CodePage437Font");
    for (int i = 0; i < 16 * 10; ++i) {
        panic_font[i] = code_page_437[((int) '0') * 16 + i];
    }
    for (int i = 0; i < 16; ++i) {
        panic_font[i + 16 * 10] = code_page_437[((int) '_') * 16 + i];
    }
    for (int i = 0; i < 16 * 26; ++i) {
        panic_font[i + 16 * 11] = code_page_437[((int) 'A') * 16 + i];
    }

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
    
    SCREEN_WIDTH = (data->width - 128) / 8 - 4;
    SCREEN_HEIGHT = (data->height - (256 - 48)) / 16 - 2;

    LogWriteSerial("%d x %d. depth = %d\n", vesa_width, vesa_height, vesa_depth);

    data->framebuffer_virtual = (uint8_t*) MapVirt(data->framebuffer_physical, 0, data->pitch * data->height, VM_LOCK | VM_MAP_HARDWARE | VM_READ | VM_WRITE, NULL, 0);
    LogWriteSerial("Framebuffer is at 0x%X\n", data->framebuffer_virtual);
    
    data->cursor_x = 0;
    data->cursor_y = 0;

    struct video_driver driver;
    driver.putchar = DrvConsolePutchar;
    driver.puts = DrvConsolePuts;
    InitVideoConsole(driver);

    SetGraphicalPanicHandler(PanicHandler);
    CreateThread(ShowRAMUsage, NULL, GetVas(), "ram usage");

    /*int LINES_PER_SECTION = 50;
    auto HsvToRgb = (uint32_t (*)(int, int, int)) GetSymbolAddress("HsvToRgb");

    for (int y = 0; y < vesa_height; ++y) {
        for (int x = 0; x < vesa_width; ++x) {
            int hue = 359 * x / vesa_width;
            int sat = (y % LINES_PER_SECTION) * (100 / LINES_PER_SECTION);
            int val = 100 * (y / LINES_PER_SECTION) / (vesa_height / LINES_PER_SECTION);
            uint32_t col = HsvToRgb(hue, sat, val);
            VesaPutpixel(x, y, col);
        }
    }*/

    auto special_bayer = (uint8_t (*)(int, int, uint16_t)) GetSymbolAddress("GetBayerAdjustedChannelForVeryHighQuality");

    _GenericVideoPutrect(data->framebuffer_virtual, data->pitch, data->depth_in_bits, 0, 0, vesa_width, vesa_height, BGCOL /*0x3880F8*/);

    /*struct file* img_file;
    OpenFile("sys:/bwsc.img", O_RDONLY, 0, &img_file);
    uint8_t* test_image = (uint8_t*) MapVirt(0, 0, 1024 * 768 * 3, VM_READ | VM_FILE, img_file, 0);
    
    int i = 0;
    for (int y = 0; y < 768; ++y) {
        for (int x = 0; x < 1024; ++x) {
            uint32_t col = test_image[i + 2];
            col <<= 8;
            col |= test_image[i + 1];
            col <<= 8;
            col |= test_image[i];
            i += 3;
            VesaPutpixel(x, y, col);
        }
    }

    UnmapVirt((size_t) test_image, 1024 * 768 * 3);
    CloseFile(img_file);*/

    const int SEGMENT_WIDTH = 1;
    for (int x = 64; x < vesa_width - 64; x += SEGMENT_WIDTH) {
        int progress = 1000 * (x - 64) / (vesa_width - 128);
        const int SOLID_PROPORTION = 400;
        if (progress > SOLID_PROPORTION) {
            progress = (progress - SOLID_PROPORTION) * 1000 / (1000 - SOLID_PROPORTION);
        } else {
            progress = 0;
        }
        int green = progress * 0x7F / 1000;
        int blue = 0xAA + progress * (0xFF - 0xAA) / 1000;
        if (data->depth_in_bits >= 15) {
            green = special_bayer(x, 0, progress * 0x7F00 / 1000);
            blue = 0xAA + special_bayer(x, 0, progress * (0xFF00 - 0xAA00) / 1000);
        }
        _GenericVideoPutrect(data->framebuffer_virtual, data->pitch, data->depth_in_bits, x, 128 - 24, SEGMENT_WIDTH, 24, (green << 8) | blue);
    }

    _GenericVideoPutrect(data->framebuffer_virtual, data->pitch, data->depth_in_bits, 64, 128, vesa_width - 128, vesa_height - (256 - 48), 0x000000);
    const char* titlebar = "New Operating System Kernel";
    for (int i = 0; titlebar[i]; ++i) {
        _GenericVideoDrawConsoleCharacter(data->framebuffer_virtual, data->pitch, data->depth_in_bits, 64 + 16 + i * 8, 128 - 24 + 5, 0x0000AA, 0xFFFFFF, titlebar[i]);
    }
}
