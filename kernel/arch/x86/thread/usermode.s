
global ArchSwitchToUsermode

; This is only called the first time we want to switch a given
; thread into usermode. In all other cases the switch will occur
; back through an interrupt handler (e.g. after a system call completes)

ArchSwitchToUsermode:
    ; Takes in an address to a usermode address to start execution
    mov ebx, [esp + 4]

    ; And a user stack pointer
    mov ecx, [esp + 8]

    ; And initial argument. For x86, we'll just store this in EDX, and the
    ; program can just read that value.
    mov edx, [esp + 12]

    ; Usermode data segment
    mov ax, 0x23
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; We need to push the current stack as the usermode part will use it too
    push 0x23            ; Usermode stack segment
    push ecx             ; Usermode stack pointer
    push 0x202           ; Flags
    push 0x1B            ; Usermode code segment
    push ebx             ; Usermode entry point

    ; Pop all of that off the stack and go to usermode.
    iretd