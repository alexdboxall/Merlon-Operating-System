#include <machine/dev.h>
#include <machine/portio.h>
#include <driver.h>
#include <thread.h>
#include <panic.h>
#include <log.h>
#include <virtual.h>

static size_t Loadx86Driver(const char* filename, const char* init) {
    if (RequireDriver(filename)) {
        PanicEx(PANIC_REQUIRED_DRIVER_NOT_FOUND, filename);
    }
    if (GetSymbolAddress(init) != 0) {
        PanicEx(PANIC_REQUIRED_DRIVER_MISSING_SYMBOL, filename);
    }
    return addr;
}

static void LoadSlowDriversInBackground(void*) {
    ((void(*)()) (Loadx86Driver("sys:/acpi.sys", "InitAcpica")))();
}

void ArchInitDev(bool fs) {
    if (!fs) {
        InitIde();
        //InitFloppy();
        
    } else {
        ((void(*)()) (Loadx86Driver("sys:/vesa.sys", "InitVesa")))();
        ((void(*)()) (Loadx86Driver("sys:/ps2.sys", "InitPs2")))();
        CreateThread(LoadSlowDriversInBackground, NULL, GetVas(), "drvloader");
    }
}