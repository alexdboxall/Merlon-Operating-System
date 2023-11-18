
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

void ArchInitBootstrapCpu(struct cpu* cpu) {
    (void) cpu;

    x86InitGdt();
    x86InitIdt();
    x86InitTss();
    
    InitPic();
    InitPit(100);

    ArchEnableInterrupts();
    x86MakeReadyForIrqs();
}

bool ArchInitNextCpu(struct cpu* cpu) {
    (void) cpu;
    return false;
}

void ArchReboot(void) {
	uint8_t good = 0x02;
    while (good & 0x02) {
        good = inb(0x64);
	}
    outb(0x64, 0xFE);
    while (1) {
		ArchStallProcessor();
	}
}