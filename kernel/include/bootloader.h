#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define BOOTCOL_ALL_BLACK       0x00
#define BOOTCOL_GREY_ON_BLACK   0x07
#define BOOTCOL_WHITE_ON_BLUE   0x1F
#define BOOTCOL_WHITE_ON_BLACK  0x0F

#define BOOTRAM_TYPE_AVAILABLE      0
#define BOOTRAM_TYPE_RESERVED       1
#define BOOTRAM_TYPE_RECLAIMABLE    2

#define BOOTRAM_GET_TYPE(info) (info & 0xF)
struct boot_memory_entry {
    uint64_t address;
    uint64_t length;
    uint64_t info;
};

#define BOOTKEY_NONE     0
#define BOOTKEY_SPACE    ' '
#define BOOTKEY_UP       1
#define BOOTKEY_DOWN     2
#define BOOTKEY_LEFT     3
#define BOOTKEY_RIGHT    4
#define BOOTKEY_BKSP     5
#define BOOTKEY_ESCAPE   6
#define BOOTKEY_TAB      '\t'   // 9
#define BOOTKEY_ENTER    '\n'   // 10

struct firmware_info {
    size_t num_ram_table_entries;
    struct boot_memory_entry* ram_table;
    size_t kernel_load_point;
    size_t reserved;
    char kernel_filename[32];
    void (*putchar)(int x, int y, char c, uint8_t col);
    int (*check_key)(void);
    void (*reboot)(void);
    void (*sleep_100ms)(void);
    int (*get_file_size)(const char* filename, size_t* size);
    int (*read_file)(const char* filename, void* buffer);
    void (*exit_firmware)(void);
} __attribute__((packed));  

struct boot_loaded_module {
    char name[48];
    uint64_t address;
    uint64_t length;
} __attribute__((packed));

#define BOOTVIDEO_GET_BPP(info)  (info & 0xFF)
#define BOOTVIDEO_VGA_TEXT(info) ((info >> 8) & 1) 
#define BOOTVIDEO_VGA_16(info)   ((info >> 9) & 1) 
#define BOOTVIDEO_BANKED(info)   ((info >> 10) & 1)

struct boot_video_mode {
    size_t width;
    size_t height;
    size_t pitch;
    size_t info;
} __attribute__((packed));

struct boot_video {
    size_t current_mode;
    size_t total_modes;

    void* framebuffer;

    struct boot_video_mode* all_modes;
} __attribute__((packed));

struct kernel_boot_info {
    size_t num_ram_table_entries;
    size_t num_loaded_modules;

    struct boot_memory_entry* ram_table;
    struct boot_loaded_module* modules;

    size_t argc;
    char* argv;

    /* 
     * We can use this information to free the memory used by the bootloader
     * (e.g. for argv, the RAM table, etc.).
     */
    size_t bootloader_area_base;
    size_t bootloader_area_size;

    struct boot_video_information* video;

    size_t partition_id;
    size_t boot_medium;
    size_t firmware_type;

} __attribute__((packed));