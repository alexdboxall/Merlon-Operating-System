
#include <machine/pic.h>
#include <machine/pit.h>
#include <machine/portio.h>
#include <arch.h>
#include <assert.h>
#include <common.h>
#include <timer.h>
#include <irq.h>
#include <log.h>

/*
* x86/dev/pit.c - Programmable Interval Timer
*
* A system timer which can generate an interrupt (on IRQ 0) at regular intervals.
*/

static uint64_t pit_hertz = 0;
static uint64_t pit_nanos = 0;

static int HandlePit(struct x86_regs* r) {
    (void) r;
    assert(pit_nanos != 0);

    ReceivedTimer(pit_nanos);

    return 0;
}

void InitPit(int hertz) { 
	pit_hertz = (uint64_t) hertz;

	int divisor = 1193180 / hertz;
	outb(0x43, 0x36);
	outb(0x40, divisor & 0xFF);
	outb(0x40, divisor >> 8);

    pit_nanos = 1000000000ULL / pit_hertz;

    RegisterIrqHandler(PIC_IRQ_BASE + 0, HandlePit);
}
