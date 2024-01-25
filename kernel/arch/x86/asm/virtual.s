
;
; x86/asm/virtual.s - Virtual Memory Helpers
;
; Provides helper functions for accessing registers involved in virtual memory.
;

global x86GetCr2
global x86SetCr3

x86GetCr2:
    mov eax, cr2
    ret

x86SetCr3:
    mov eax, [esp + 4]
    mov cr3, eax
    ret