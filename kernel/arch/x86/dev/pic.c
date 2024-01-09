#include <machine/pic.h>
#include <machine/portio.h>
#include <arch.h>

#define PIC1_COMMAND    0x20
#define PIC1_DATA       0x21
#define PIC2_COMMAND    0xA0
#define PIC2_DATA       0xA1

#define PIC_EOI         0x20
#define PIC_REG_ISR     0x0B

#define ICW1_ICW4       0x01
#define ICW1_INIT       0x10
#define ICW4_8086       0x01

/*
* Delay for a short period of time, for use in betwwen IO calls to the PIC.
* This is required as some PICs have a hard time keeping up with the speed 
* of modern CPUs (the original PIC was introduced in 1976!).
*/
static void IoWait(void) {
    asm volatile ("nop");
}

/*
* Read an internal PIC register.
*/
static uint16_t ReadPicReg(int ocw3) {
    outb(PIC1_COMMAND, ocw3);
    outb(PIC2_COMMAND, ocw3);
    return ((uint16_t) inb(PIC2_COMMAND) << 8) | inb(PIC1_COMMAND);
}

/*
* Due to a race condition between the PIC and the CPU, we sometimes get a
* 'spurious' interrupt sent to the CPU on IRQ 7 or 15. Distinguishing them is 
* important - we don't need to send an EOI after a spurious interrupt.
*/
bool IsPicIrqSpurious(int irq_num) {
    if (irq_num == PIC_IRQ_BASE + 7) {
        uint16_t isr = ReadPicReg(PIC_REG_ISR);
        return !(isr & (1 << 7));

    } else if (irq_num == PIC_IRQ_BASE + 15) {
        uint16_t isr = ReadPicReg(PIC_REG_ISR);
        if (!(isr & (1 << 15))) {
            /*
            * It is spurious, but the primary PIC doesn't know that, as it came
            * from the secondary PIC. So only send an EOI to the primary PIC.
            */
            outb(PIC1_COMMAND, PIC_EOI);
            return true;
        }
    }
    
    return false;
}

/*
* Acknowledge the previous interrupt. We will not receive any interrupts of
* the same type until we have acknowledged it.
*/
void SendPicEoi(int irq_num) {
    if (irq_num >= PIC_IRQ_BASE + 8) {
        outb(PIC2_COMMAND, PIC_EOI);
    }
    outb(PIC1_COMMAND, PIC_EOI);
}

/*
* Change which interrupt numbers are used by the IRQs. They will initially
* use interrupts 0 through 15, which isn't very good as it conflicts with the
* interrupt numbers for the CPU exceptions. 
*/
static void RemapPic(int offset) {
    uint8_t mask1 = inb(PIC1_DATA);
    uint8_t mask2 = inb(PIC2_DATA);

    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    IoWait();
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    IoWait();
    outb(PIC1_DATA, offset);
    IoWait();
    outb(PIC2_DATA, offset + 8);
    IoWait();
    outb(PIC1_DATA, 4);
    IoWait();
    outb(PIC2_DATA, 2);
    IoWait();

    outb(PIC1_DATA, ICW4_8086);
    IoWait();
    outb(PIC2_DATA, ICW4_8086);
    IoWait();

    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);
}

/**
 * Set which IRQ numbers are disabled. Overwrites the previous call to this
 * function completely (i.e. this is 'equals', not an 'and' or 'or'.) To disable
 * all lines, specify 0xFFFF. To enable all lines, specify 0x0000.
 */
void DisablePicLines(uint16_t irq_bitfield) {
    static uint16_t prev = 0xFFFF;

    if (prev != irq_bitfield) {
        outb(PIC1_DATA, irq_bitfield & 0xFF);
        outb(PIC2_DATA, irq_bitfield >> 8);
        prev = irq_bitfield;
    }
}

void InitPic(void) {
    RemapPic(PIC_IRQ_BASE);
    DisablePicLines(0x0000);
}
