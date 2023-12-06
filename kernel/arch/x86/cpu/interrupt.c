
#include <machine/regs.h>
#include <machine/interrupt.h>
#include <machine/pic.h>
#include <log.h> 
#include <irq.h>
#include <irql.h>
#include <virtual.h>
#include <console.h>

static bool ready_for_irqs = false;

static int GetRequiredIrql(int irq_num) {
    if (irq_num == PIC_IRQ_BASE + 0) {
        return IRQL_TIMER;
    } else {
        return IRQL_DRIVER + irq_num - PIC_IRQ_BASE;
    }
}

void x86HandleInterrupt(struct x86_regs* r) {
    int num = r->int_no;

    if (num >= PIC_IRQ_BASE && num < PIC_IRQ_BASE + 16) {
        RespondToIrq(num, GetRequiredIrql(num), r);

    } else if (num == 14) {
        extern size_t x86GetCr2();

        int type = 0;
        if (r->err_code & 1) {
            type |= VM_READ;
        }
        if (r->err_code & 2) {
            type |= VM_WRITE;
        }
        if (r->err_code & 4) {
            type |= VM_USER;
        }
        if (r->err_code & 16) {
            type |= VM_EXEC;
        }

        LogWriteSerial("Page fault: cr2 0x%X, eip 0x%X, nos-err 0x%X\n", x86GetCr2(), r->eip, type);

        HandleVirtFault(x86GetCr2(), type);
    }

    /*
    if (protection fault, etc. || page fault unhandled) {
        RespondToUnhandledFault
    } else if (syscall) {
        RespondToSyscall(...)
    }

    */
}

void ArchSendEoi(int irq_num) {
    SendPicEoi(irq_num);
}

void ArchSetIrql(int irql) {
    if (irql == IRQL_HIGH || irql == IRQL_TIMER || !x86IsReadyForIrqs()) {
        /*
         * Interrupts stay off.
         */
        return;
    }
    
    if (irql >= IRQL_DRIVER) {
        //int irq_num = irql - IRQL_DRIVER;

        /*
         * We want to disable all higher IRQs (as the PIC puts the lowest priority interrupts at 
         * high numbers), as well as our self. Allow IRQ2 to stay enabled as it is used internally.
         */
        //uint16_t mask = (0xFFFF ^ ((1 << irq_num) - 1)) & ~(1 << 2);
        //DisablePicLines(mask);

        /*
         * e.g. lets say we got IRQ6 - floppy disk.
         * we want bits 0-5 to be clear, and bits 6-15 to be set.
         * 
         * irq_num                          = 6
         * (1 << irq_num)                   = 64        = 0b 0000 0000 0100 0000
         * (1 << irq_num) - 1               = 63        = 0b 0000 0000 0011 1111
         * 0xFFFF ^ ((1 << irq_num) - 1)    = X         = 0b 1111 1111 1100 0000
         * 
         * 1 << 2                           =           = 0b 0000 0000 0000 0100
         * ~(1 << 2)                        = Y         = 0b 1111 1111 1111 1011
         * 
         * X & Y                            = MASK      = 0b 1111 1111 1100 0000
         * 
         * So we got what we wanted.
         * 
         * On IRQ1:
         * 
         * 0xFFFF ^ ((1 << irq_num) - 1)    = X         = 0b 1111 1111 1111 1110
         * ~(1 << 2)                        = Y         = 0b 1111 1111 1111 1011
         * X & Y                            = MASK      = 0b 1111 1111 1111 1010
         */

    } else {
        /*
         * Allow everything to go through.
         */
        //DisablePicLines(0x0000);
    }

    ArchEnableInterrupts();
}

bool x86IsReadyForIrqs(void) {
    return ready_for_irqs;
}

void x86MakeReadyForIrqs(void) {
    ready_for_irqs = true;
    RaiseIrql(GetIrql());
}