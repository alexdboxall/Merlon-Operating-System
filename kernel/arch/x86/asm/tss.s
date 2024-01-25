
;
; x86/asm/tss.s - Task State Segment
;
; TODO: ...
;

global x86LoadTss
x86LoadTss:
    mov eax, [esp + 4]
    ltr ax
    ret