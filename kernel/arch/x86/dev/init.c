#include <machine/dev.h>
#include <machine/cmos.h>
#include <machine/portio.h>
#include <driver.h>
#include <thread.h>
#include <panic.h>
#include <log.h>
#include <virtual.h>
#include <timeconv.h>
#include <bootloader.h>

static size_t Loadx86Driver(const char* filename, const char* init) {
    if (RequireDriver(filename)) {
        PanicEx(PANIC_REQUIRED_DRIVER_NOT_FOUND, filename);
    }
    size_t addr = GetSymbolAddress(init);
    if (addr == 0) {
        return GetDriverAddress(filename);
    }
    return addr;
}

static void LoadSlowDriversInBackground(void*) {
    ((void(*)()) (Loadx86Driver("sys:/acpi.sys", "InitAcpica")))();
}

void ArchInitDev(bool fs) {
    if (!fs) {
        struct kernel_boot_info boot = GetBootInformation();
        InitIde();
        if (boot.enable_floppy) {
            InitFloppy();
        }

        uint64_t utct = ArchGetUtcTime(0);
        LogWriteSerial("UTC time = 0x%X 0x%X\n", (uint32_t) utct, (uint32_t) (utct >> 32));

        struct ostime splitt = TimeValueToStruct(utct);
        LogWriteSerial("%d:%d:%d %d/%d/%d", splitt.hour, splitt.min, splitt.sec, splitt.day, splitt.month, splitt.year);

        utct = TimeStructToValue(splitt);
        LogWriteSerial("reconverted time = 0x%X 0x%X\n", (uint32_t) utct, (uint32_t) (utct >> 32));

    } else {
        ((void(*)()) (Loadx86Driver("sys:/vga.sys", "InitVga")))();
        ((void(*)()) (Loadx86Driver("sys:/ps2.sys", "InitPs2")))();
        CreateThread(LoadSlowDriversInBackground, NULL, GetVas(), "drvloader");
    }
}