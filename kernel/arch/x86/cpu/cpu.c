
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

void ArchInitBootstrapCpu(struct cpu*) {
    x86InitGdt();
    x86InitIdt();
    x86InitTss();
    
    InitPic();
    InitPit(40);

    ArchEnableInterrupts();
    x86MakeReadyForIrqs();
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
    /*
     * These ones work on emulators/hypervisors only. Once we get ACPICA, we'll put a little code here to
     * detect if ACPICA.SYS has been loaded, and we'll do a proper shutdown instead.
     */
    outw(0xB004, 0x2000);       // Bochs and old QEMU
    outw(0x0604, 0x2000);       // New QEMU
    outw(0x4004, 0x3400);       // VirtualBox
    outw(0x0600, 0x0034);       // Cloud Hypervisor
}

int ArchSetPowerState(int power_state) {
	switch (power_state) {
    case ARCH_POWER_STATE_REBOOT:
        x86Reboot();
        break;
    case ARCH_POWER_STATE_SHUTDOWN:
        x86Shutdown();
        break;
    default:
        return EINVAL;
    }

    while (1) {
		ArchStallProcessor();
	}
}