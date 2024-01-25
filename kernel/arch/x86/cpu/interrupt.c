
#include <machine/regs.h>
#include <machine/interrupt.h>
#include <machine/pic.h>
#include <log.h> 
#include <irq.h>
#include <irql.h>
#include <virtual.h>
#include <syscall.h>
#include <panic.h>
#include <console.h>

#define ISR_SYSTEM_CALL 96
#define ISR_PAGE_FAULT  14
#define ISR_NMI         2

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

    } else if (num == ISR_PAGE_FAULT) {
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

        LogWriteSerial("\n\nPage fault: cr2 0x%X, eip 0x%X, nos-err 0x%X\n", 
            x86GetCr2(), r->eip, type
        );
        HandleVirtFault(x86GetCr2(), type);

    } else if (num == ISR_NMI) {
        Panic(PANIC_NON_MASKABLE_INTERRUPT);

    } else if (num == ISR_SYSTEM_CALL) {
        r->eax = HandleSystemCall(r->eax, r->ebx, r->ecx, r->edx, r->esi, r->edi);

    } else {
        LogWriteSerial("Got interrupt %d. (r->eip = 0x%X)\n", num, r->eip);
        UnhandledFault();
    }
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
        int irq_num = irql - IRQL_DRIVER;

        /*
         * We want to disable all higher IRQs (as the PIC puts the lowest 
         * priority interrupts at high numbers), as well as our self. Allow IRQ2
         * to stay enabled as it is used internally.
         */
        uint16_t mask = (0xFFFF ^ ((1 << irq_num) - 1)) & ~(1 << 2);
        DisablePicLines(mask);

    } else {
        /*
         * Allow everything to go through.
         */
         DisablePicLines(0x0000);
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