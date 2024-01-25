
;
; x86/asm/interrupt.s - Interrupt Handling
;
; Provides the common handler function for all of the different interrupts the
; CPU will receive. The specific handlers are generated in x86/cpu/vectors.py.
;

extern x86HandleInterrupt
global InterruptCommonHandler
InterruptCommonHandler:
    pushad
	push ds
    push es
    push fs
    push gs
	
    mov ax, 0x10
	mov ds, ax
    mov es, ax

    ; Do NOT enable interrupts here - let the Irql stuff work its magic. Unholy
    ; demons will haunt you if you do (e.g. corrupting `cr2` if a timer 
    ; interrupt happens to switch one page-faulting thread to another one which
    ; is just about to page fault (when it switches back to the first, `cr2`
    ; would have been overwritten with the one for the second). 
	
    push esp
	cld
    call x86HandleInterrupt

    add esp, 4
    pop gs
    pop fs
    pop es
    pop ds
    popad

    ; Skip over the error code and interrupt number (we can't pop them
    ; anywhere, as the registers have already been restored)
    add esp, 8

    iretd