
#include <machine/regs.h>
#include <machine/interrupt.h>
#include <machine/portio.h>
#include <machine/cmos.h>
#include <log.h> 
#include <irq.h>
#include <irql.h>
#include <virtual.h>
#include <syscall.h>
#include <panic.h>
#include <console.h>

void HandleNmi(void) {
    SetNmiEnable(false);
    ArchDisableInterrupts();
    LogDeveloperWarning("[cpu]: Non-maskable interrupt occured.\n");

    uint8_t sys_ctrl_b = inb(0x61);
    if (sys_ctrl_b & 0x40) {
        Panic(PANIC_BUS_FAULT);
    } else if (sys_ctrl_b & 0x80) {
        Panic(PANIC_MEMORY_FAULT);
    } else {
        PanicEx(PANIC_BUS_FAULT, "unknown NMI");
    }

    while (true) {
        ArchStallProcessor();
        ArchDisableInterrupts();
    }
}