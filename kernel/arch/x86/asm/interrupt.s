
;
; x86/asm/interrupt.s - Interrupt Handling
;
; Provides the common handler function for all of the different interrupts the
; CPU will receive. The specific handlers are generated in x86/cpu/vectors.py.
;

extern x86HandleInterrupt
extern FindSignalToHandle
extern HandleSignal

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

    call FindSignalToHandle
    cmp eax, -1
    jne x86HandleSignals

noSignalHandler:
    pop gs
    pop fs
    pop es
    pop ds
    popad

    ; Skip over the error code and interrupt number (we can't pop them
    ; anywhere, as the registers have already been restored)
    add esp, 8

    iretd


x86HandleSignals:
    ; Ensure that it's an IRQ that actually came from userland,
    ; and not a nested kernel interrupt. Otherwise the user IRET
    ; frame is actually a long way up and we've got no way to find
    ; it.
    cmp dword [esp + 12], 0x23     ; check DS
    jne noSignalHandler

    mov ebx, eax
    push eax
    call HandleSignal
    add esp, 4
    ; EAX now has the address of the signal handler stub in userland
    ; EBX now has the signal number
    test eax, eax
    jz noSignalHandler

    cli

    ;add esp, (16 + 32 + 8 + 20)  ; skip segs, 'popad', error code and IRQ#, and IRETD
    mov edx, [esp + 72 - 4]     ; user stack pointer
    mov ecx, [esp + 72 - 16]    ; user code pointer
    mov edi, esp

    ; TODO: it's really bad that we're just trusting the user stack
    ; and pushing to it while we're running in kernel mode.

    ; - we should generate this frame on an entirely new stack
    ; - OR: do the switch to usermode but just jump to more code
    ;       in 'the kernel' (but mapped as user accessible), which
    ;       then does the pushing

    ; MOVE TO USER STACK POINTER
    xchg esp, edx
    add edx, 76

    ; pop 4 segments
    ; pop 8 GP registers
    ; pop flags
    ; pop 2 args (addr + sig num)
    ; pop 1 return address (to x86FinishSignal)

    push ecx
    push dword [edx - 12]         ; flags

    push dword [edx - 60]         ; PUSHAD
    push dword [edx - 56]        
    push dword [edx - 52]        
    push dword [edx - 48]        
    push dword [edx - 44]        
    push dword [edx - 40]        
    push dword [edx - 36]        
    push dword [edx - 32]

    push dword [edx - 76]         ; PUSH SEGS
    push dword [edx - 72]
    push dword [edx - 68]
    push dword [edx - 64]
    
    push ebx

    ; MOVE BACK TO KERNEL STACK
    xchg esp, edi
    push 0x23            ; Usermode stack segment
    push edi             ; Usermode stack pointer
    push 0x202           ; Flags
    push 0x1B            ; Usermode code segment
    push eax             ; Usermode entry point
    iretd
    ;jmp ecx

    ; SEGS                                              -64 (GS), -68 (ES), -72 (FS), -76 (DS)
    ; POPAD                                             -32 (EDI), -36, -40, -44, -48, -52, -56, -60 (EAX)
    ; IRQ#                                              -28
    ; ERR (or vice versa)                               -24
    ; push 0x23            ; Usermode stack segment     -20 
    ; push ecx             ; Usermode stack pointer     -16
    ; push 0x202           ; Flags                      -12
    ; push 0x1B            ; Usermode code segment      -8     
    ; push ebx             ; Usermode entry point       -4

; AN EXAMPLE SIGNAL HANDLER IN USERSPACE THAT WE SHOULD JUMP TO:
;
; CommonSighandlerx86:
;       call CommonSignalHandler        ;args already in correct spot
;       add esp, 8                      ; remove args
;       pop gs
;       pop fs
;       pop es
;       pop ds
;       popad
;       popf
;       ret                             ; back to 'x86FinishSignal'

