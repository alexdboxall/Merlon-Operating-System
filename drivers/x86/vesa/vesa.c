#include <heap.h>
#include <assert.h>
#include <string.h>
#include <virtual.h>
#include <errno.h>
#include <log.h>
#include <vfs.h>
#include <voidptr.h>
#include <thread.h>
#include <physical.h>
#include <common.h>
#include <semaphore.h>
#include <video.h>
#include <driver.h>
#include <fcntl.h>
#include <irql.h>

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
static struct vesa_data* data = NULL;

void (*GenericVideoDrawConsoleCharacter)(uint8_t*, int, int, int, int, uint32_t, uint32_t, char) = NULL;
void (*GenericVideoPutpixel)(uint8_t*, int, int, int, int, uint32_t);
void (*GenericVideoPutrect)(uint8_t*, int, int, int, int, int, int, uint32_t);

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
        GenericVideoPutrect(data->framebuffer_virtual, data->pitch, data->depth_in_bits, 
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
    GenericVideoPutpixel(data->framebuffer_virtual, data->pitch, data->depth_in_bits, x, y, colour);
}

static void vesa_render_character(char c) {
    GenericVideoDrawConsoleCharacter(
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
    LogWriteSerial("DrvConsolePutchar A\n");
    AcquireMutex(data_lock, -1);
    LogWriteSerial("DrvConsolePutchar B\n");

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
        num_str[4] = ((num / 10) % 10) + '0';
        num_str[5] = (num % 10) + '0';

    } else {
        strcat(str, "99999");
        return;
    }

    strcat(str, num_str);
}

/*
d0000398: 50                           	push	eax
d0000399: 6a 00                        	push	0
d000039b: 68 db b9 0b 00               	push	768475
d00003a0: 6a 40                        	push	64
d00003a2: 56                           	push	esi
d00003a3: ff 72 24                     	push	dword ptr [edx + 36]
d00003a6: ff 72 1c                     	push	dword ptr [edx + 28]
d00003a9: ff 72 14                     	push	dword ptr [edx + 20]
d00003ac: ff 15 04 20 00 d0            	call	dword ptr [-805298172]
			d00003ae:  R_386_32	GenericVideoDrawConsoleCharacter
*/

void ShowRAMUsage(void*) {
    while (true) {
        SleepMilli(250);

        size_t free = GetFreePhysKilobytes();
        size_t total = GetTotalPhysKilobytes();

        char buffer[128];
        memset(buffer, 0, 128);

        AppendNumberToStringX(buffer, total - free);
        strcat(buffer, " / ");
        AppendNumberToStringX(buffer, total);
        strcat(buffer, " KB used (");
        AppendNumberToStringX(buffer, 100 * free / total);
        strcat(buffer, "% free)             ");
    
        for (int i = 0; buffer[i]; ++i) {
            AcquireMutex(data_lock, -1);
            GenericVideoDrawConsoleCharacter(
                data->framebuffer_virtual, 
                data->pitch, 
                data->depth_in_bits, 
                64 + i * 8, 
                64, 
                0x0bb9db, 
                0x000000, 
                buffer[i]
            );
            ReleaseMutex(data_lock);
        }   
    }
}

/*
* Set up the hardware, and install the device into the virtual filesystem.
*/
void InitVesa(void) {
    data_lock = CreateMutex("vesa");
    LogWriteSerial("data_lock is 0x%X, and located at 0x%X\n", data_lock, &data_lock);
    
    RequireDriver("sys:/cmnvideo.sys");
    GenericVideoPutpixel = (void (*)(uint8_t*, int, int, int, int, uint32_t)) GetSymbolAddress("GenericVideoPutpixel");
    GenericVideoDrawConsoleCharacter = (void (*)(uint8_t*, int, int, int, int, uint32_t, uint32_t, char)) GetSymbolAddress("GenericVideoDrawConsoleCharacter");
    GenericVideoPutrect = (void (*)(uint8_t*, int, int, int, int, int, int, uint32_t)) GetSymbolAddress("GenericVideoPutrect");

    //auto HsvToRgb = (uint32_t (*)(int, int, int)) GetSymbolAddress("HsvToRgb");

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

    auto special_bayer = (uint8_t (*)(int, int, uint16_t)) GetSymbolAddress("GetBayerAdjustedChannelForVeryHighQuality");

    GenericVideoPutrect(data->framebuffer_virtual, data->pitch, data->depth_in_bits, 0, 0, vesa_width, vesa_height, 0x0bb9db /*0x3880F8*/);

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
        GenericVideoPutrect(data->framebuffer_virtual, data->pitch, data->depth_in_bits, x, 128 - 24, SEGMENT_WIDTH, 24, (green << 8) | blue);
    }

    GenericVideoPutrect(data->framebuffer_virtual, data->pitch, data->depth_in_bits, 64, 128, vesa_width - 128, vesa_height - (256 - 48), 0x000000);

    CreateThread(ShowRAMUsage, NULL, GetVas(), "ram usage");

    /*struct open_file* img_file;
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
}
