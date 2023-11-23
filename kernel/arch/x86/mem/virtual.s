global x86GetCr2
global x86SetCr3

x86GetCr2:
    mov eax, cr2
    ret

x86SetCr3:
    mov eax, [esp + 4]
    mov cr3, eax
    ret