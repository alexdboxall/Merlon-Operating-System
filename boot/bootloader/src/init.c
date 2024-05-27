#include <bootloader.h>
#include <elf.h>
#include <util.h>

#define ENTRY_POINT __attribute__((__section__(".entrypoint")))

struct kernel_boot_info kboot_info;

struct firmware_info* firmware;
struct firmware_info* GetFw(void) {
    return firmware;
}

void* xmemset(void* addr, int c, size_t n)
{
    return __builtin_memset(addr, c, n);
}

void* xmemcpy(void* restrict dst, const void* restrict src, size_t n)
{
    return __builtin_memcpy(dst, src, n);
}

static void HideBootOptionsMessage(void) {
    for (int i = 0; i < 40; ++i) {
        Putchar(2 + i, 23, ' ', BOOTCOL_ALL_BLACK);
    }
}

static void DrawBootMessage() {
    static int dot_cycle = 0;
    for (int j = 0; j < 3; ++j) {
        Putchar(13 + j, 1, (dot_cycle % 4) > j ? '.' : 0, BOOTCOL_WHITE_ON_BLACK);
    }
    ++dot_cycle;
}

static void DisplayBootingHeader(void) {
    Clear();
    SetCursor(2, 1);
    Puts("Booting NOS", BOOTCOL_WHITE_ON_BLACK);
}

static void ShowRamTable(void) {
    Printf("  The RAM table has %d entries:\n  ", GetFw()->num_ram_table_entries);
    for (size_t i = 0; i < GetFw()->num_ram_table_entries; ++i) {
        struct boot_memory_entry ram = GetFw()->ram_table[i];
        int type = BOOTRAM_GET_TYPE(ram.info);

        Printf("      [%s]: 0x%X -> 0x%X (len: 0x%X)\n  ", 
            type == BOOTRAM_TYPE_AVAILABLE ? "AVAIL" :
            (type == BOOTRAM_TYPE_RECLAIMABLE ? "ACPI " : "RESV "),
            (size_t) ram.address,
            (size_t) (ram.address + ram.length - 1),
            (size_t) ram.length
        ); 
    }
}

static void BootOptionsMenu(void) {
    const char* options[] = {
        "[1] - Continue booting",
        "[2] - Reboot",
        "",
        "[R] - Show RAM table",        
        NULL
    };

    while (true) {
        Clear();
        SetCursor(2, 1);
        Puts("Boot Options", BOOTCOL_WHITE_ON_BLACK);
        SetCursor(2, 3);
        Puts("Please select an option by pressing that key on your keyboard.", BOOTCOL_GREY_ON_BLACK);

        int i = 0;
        while (options[i] != NULL) {
            SetCursor(4, 5 + i); 
            Puts(options[i], BOOTCOL_GREY_ON_BLACK);
            ++i;
        }
        SetCursor(4, 5 + i);
        if (kboot_info.enable_floppy) {
            Puts("[F] - Disable floppy drive", BOOTCOL_GREY_ON_BLACK);
        } else {
            Puts("[F] - Enable floppy drive", BOOTCOL_GREY_ON_BLACK);
        }

retry:;
        char key = WaitKey();
        if (key == '1') {
            break;
        } else if (key == '2') {
            Reboot();
        } else if (key == 'R' || key == 'r') {
            Clear();
            SetCursor(2, 1);
            Puts("RAM Table\n\n", BOOTCOL_WHITE_ON_BLACK);
            ShowRamTable();
            Puts("\n  Press the ESC key to continue.", BOOTCOL_WHITE_ON_BLACK);
            while (CheckKey() != BOOTKEY_ESCAPE) {
                ;
            }
        } else if (key == 'F' || key == 'f') {
            kboot_info.enable_floppy ^= 1;
        } else {
            goto retry;
        }
    }

    DisplayBootingHeader();
    Puts("...", BOOTCOL_WHITE_ON_BLACK);
}

static bool DisplayBootScreen(void) {
    DisplayBootingHeader();
    SetCursor(2, 23);
    Puts("Press the ESC key to load boot options", BOOTCOL_GREY_ON_BLACK);

    for (int i = 0; i < 4; ++i) {
        DrawBootMessage(i);
        Sleep(200);
        if (CheckKey() == BOOTKEY_ESCAPE) {
            HideBootOptionsMessage();
            return true;
        }
    }
    HideBootOptionsMessage();
    return false;
}

static size_t LoadProgramHeaders(size_t base) {
    size_t krnl_dest = GetFw()->kernel_load_point;
	struct Elf32_Ehdr* elf = (struct Elf32_Ehdr*) base;
	struct Elf32_Phdr* progHeaders = (struct Elf32_Phdr*) (base + elf->e_phoff);

	for (uint16_t i = 0; i < elf->e_phnum; ++i) {
		size_t addr = (progHeaders + i)->p_vaddr;
		size_t fileOffset = (progHeaders + i)->p_offset;
		size_t size = (progHeaders + i)->p_filesz;

		if ((progHeaders + i)->p_type == PHT_LOAD) {
			void* filePos = (void*) (base + fileOffset);

			uint32_t additionalNullBytes = (progHeaders + i)->p_memsz - (progHeaders + i)->p_filesz;

            DiagnosticPrintf("Loading here: 0x%X \n  ", addr & 0xFFFFFFF);
			xmemcpy(addr & 0xFFFFFFF, filePos, size);
			xmemset((addr & 0xFFFFFFF) + size, 0, additionalNullBytes);
		}
	}

    return elf->e_entry;
}

void ENTRY_POINT InitBootloader(struct firmware_info* fw) {
    firmware = fw;

    bool show_boot_options = DisplayBootScreen();
    
    SetCursor(2, 3);
    DiagnosticPrintf("The kernel file is: %s\n  ", fw->kernel_filename);

    if (!DoesFileExist(fw->kernel_filename)) {
        Printf("The kernel file doesn't exist!\n  ");
        while (true) {;}
    }
    
    size_t file_size = GetFileSize(fw->kernel_filename);
    DiagnosticPrintf("The kernel file exists, and has a size of %d.%d KiB\n  ", file_size / 1024, (file_size % 1023) * 10 / 1024);

    LoadFile(fw->kernel_filename, 0x10000);
    DiagnosticPrintf("The kernel image has been loaded into RAM.\n  ");

    size_t entry_point = LoadProgramHeaders(0x10000);
    DiagnosticPrintf("The kernel's executable has been fully loaded to address 0x%X.\n  ", fw->kernel_load_point);
    DiagnosticPrintf("The kernel entry point is at 0x%X\n  ", entry_point);

    kboot_info.enable_floppy = 1;

    if (show_boot_options) {
        BootOptionsMenu();
    }

    ExitBootServices();

    kboot_info.num_loaded_modules = 0;
    kboot_info.num_ram_table_entries = fw->num_ram_table_entries;
    kboot_info.ram_table = fw->ram_table;

    ((void(*)(struct kernel_boot_info*)) entry_point)(&kboot_info);
}
