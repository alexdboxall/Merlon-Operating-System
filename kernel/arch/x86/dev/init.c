#include <machine/dev.h>
#include <driver.h>
#include <panic.h>
#include <log.h>

void ArchInitDev(bool fs) {
    if (!fs) {
        InitIde();

    } else {
        LogWriteSerial("about to load ps2.sys\n");
        int res = RequireDriver("sys:/ps2.sys");
        if (res != 0) {
            PanicEx(PANIC_DRIVER_FAULT, "can't load ps2.sys");
        }
        size_t addr = GetSymbolAddress("InitPs2");
        if (addr == 0) {
            PanicEx(PANIC_DRIVER_FAULT, "can't find InitPs2");
        }

        ((void(*)()) addr)();
    }
}