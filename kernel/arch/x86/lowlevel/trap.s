
; Our common interrupt handler
extern x86HandleInterrupt
global int_common_handler
int_common_handler:
    ; Save the registers and segments
    pushad
	push ds
    push es
    push fs
    push gs
	
    ; Ensure we have kernel segments and not user segments
    mov ax, 0x10
	mov ds, ax
    mov es, ax
    
    ; Allow nested interrupts
    sti
	
    ; Push a pointer to the registers to the kernel handler
    push esp
	cld
    call x86HandleInterrupt

    ; Restore registers
    add esp, 4
    pop gs
    pop fs
    pop es
    pop ds
    popad

    ; Skip over the error code and interrupt number (we can't pop them
    ; anywhere, as the registers have already been restored)
    add esp, 8

    ; Return from the interrupt - also restores the stack and the flags
    iretd