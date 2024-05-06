
#include <stdbool.h>
#include <virtual.h>
#include <machine/gdt.h>
#include <machine/idt.h>
#include <machine/tss.h>
#include <machine/pic.h>
#include <machine/pit.h>
#include <cpu.h>
#include <machine/portio.h>
#include <machine/interrupt.h>
#include <errno.h>
#include <driver.h>

static void x86EnableNMIs(void) {
    outb(0x70, inb(0x70) & 0x7F);
    inb(0x71);
}

void ArchInitBootstrapCpu(struct cpu*) {
    x86InitGdt();
    x86InitIdt();
    x86InitTss();
    
    InitPic();
    InitPit(40);

    ArchEnableInterrupts();
    x86MakeReadyForIrqs();
    x86EnableNMIs();
}

bool ArchInitNextCpu(struct cpu*) {
    return false;
}

static void x86Reboot(void) {
    uint8_t good = 0x02;
    while (good & 0x02) {
        good = inb(0x64);
	}
    outb(0x64, 0xFE);
}

static void x86Shutdown(void) {
    size_t acpicaShutdown = GetSymbolAddress("AcpicaShutdown");
    if (acpicaShutdown != 0) {
        ((void (*)(void)) acpicaShutdown)();
    }

    /*
     * Some emulators have ways of doing a shutdown if we don't have ACPI 
     * support yet.
     */
    outw(0xB004, 0x2000);       // Bochs and old QEMU
    outw(0x0604, 0x2000);       // New QEMU
    outw(0x4004, 0x3400);       // VirtualBox
    outw(0x0600, 0x0034);       // Cloud Hypervisor
}

static void x86Sleep(void) {
    size_t acpicaSleep = GetSymbolAddress("AcpicaSleep");
    if (acpicaSleep != 0) {
        ((void (*)(void)) acpicaSleep)();
    }
}

int ArchSetPowerState(int power_state) {
	switch (power_state) {
    case ARCH_POWER_STATE_REBOOT:
        x86Reboot();
        break;
    case ARCH_POWER_STATE_SHUTDOWN:
        x86Shutdown();
        break;
    case ARCH_POWER_STATE_SLEEP: {
        x86Sleep();
        break;
    }
    default:
        return EINVAL;
    }

    while (1) {
		ArchStallProcessor();
	}
}
