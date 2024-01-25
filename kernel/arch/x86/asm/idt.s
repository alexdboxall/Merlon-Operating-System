
;
; x86/asm/idt.s - Interrupt Descriptor Table
;
; Installs the interrupt descriptor table (IDT). This allows the CPU to respond
; to interrupts it receives.
;

global x86LoadIdt
x86LoadIdt:
	mov eax, [esp + 4]
	lidt [eax]
	ret
