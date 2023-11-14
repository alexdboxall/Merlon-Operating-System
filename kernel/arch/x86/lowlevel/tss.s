
;
;
; x86/lowlevel/tss.s - Task State Segment
;
; Like with the GDT and IDT, we need assembly to load the TSS using the
; special instruction 'ltr'.
;

extern x86LoadTss

x86LoadTss:
    mov eax, [esp + 4]
    ltr ax
    ret