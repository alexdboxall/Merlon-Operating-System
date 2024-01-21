#include <bootloader.h>
#include <util.h>

static int cur_x;
static int cur_y;

void SetCursor(int x, int y) {
    cur_x = x;
    cur_y = y;
}

void Putchar(int x, int y, char c, uint8_t col) {
    GetFw()->putchar(x, y, c, col);
}

void Puts(const char* str, uint8_t col) {
    for (int i = 0; str[i]; ++i) {
        if (str[i] == '\n') {
            cur_y++;
            cur_x = 0;
        } else {
            Putchar(cur_x, cur_y, str[i], col);
            cur_x++;
            if (cur_x == 80) {
                cur_y++;
                cur_x = 0;
            }
        }

        if (cur_y == 25) {
            cur_y = 0;
        }
    }
}

void Clear(void) {
    for (int y = 0; y < 25; ++y) {
        for (int x = 0; x < 80; ++x) {
            Putchar(x, y, ' ', BOOTCOL_ALL_BLACK);
        }
    }
    SetCursor(0, 0);
}

void Reboot(void) {
    GetFw()->reboot();
}

void ExitBootServices(void) {
    GetFw()->exit_firmware();    
}

int CheckKey(void) {
    return GetFw()->check_key();
}

int WaitKey(void) {
    while (true) {
        int key = CheckKey();
        if (key != BOOTKEY_NONE) {
            return key;
        }
    }
    return -1;
}

bool DoesFileExist(const char* filename) {
    size_t dummy;
    return GetFw()->get_file_size(filename, &dummy) == 0;
}

size_t GetFileSize(const char* filename) {
    size_t size;
    int res = GetFw()->get_file_size(filename, &size);
    if (res != 0) {
        Fail("couldn't read file size", filename);
    }
    return size;
}

void LoadFile(const char* filename, size_t addr) {
    int res = GetFw()->read_file(filename, (void*) addr);
    if (res != 0) {
        Fail("couldn't load file", filename);
    }
}

void Fail(const char* why1, const char* why2) {
    Clear();
    SetCursor(2, 1);
    Puts("Boot Failed!\n\n  ", BOOTCOL_WHITE_ON_BLACK);
    Puts(why1, BOOTCOL_GREY_ON_BLACK);
    if (why2 != NULL) {
        Puts(" (", BOOTCOL_GREY_ON_BLACK);
        Puts(why2, BOOTCOL_GREY_ON_BLACK);
        Puts(")", BOOTCOL_GREY_ON_BLACK);
    }

    Puts("\n\n\n\n  Press ENTER to reboot...", BOOTCOL_WHITE_ON_BLACK);

    while (true) {
        int key = WaitKey();
        if (key == BOOTKEY_ENTER) {
            Reboot();
        }
    }
}

void Sleep(int milli) {
    int ticks = (milli + 99) / 100;
    while (ticks--) {
        GetFw()->sleep_100ms();
    }
}