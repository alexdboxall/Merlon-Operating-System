
#include <machine/pic.h>
#include <machine/pit.h>
#include <machine/portio.h>
#include <arch.h>
#include <common.h>
#include <timer.h>
#include <irq.h>

static uint64_t pit_nanos = 0;

static int HandlePit(struct x86_regs*) {
    ReceivedTimer(pit_nanos);
    return 0;
}

void InitPit(int hertz) { 
	int divisor = 1193180 / hertz;
	outb(0x43, 0x36);
	outb(0x40, divisor & 0xFF);
	outb(0x40, divisor >> 8);

    pit_nanos = 1000000000ULL / hertz;
    RegisterIrqHandler(PIC_IRQ_BASE + 0, HandlePit);
}
