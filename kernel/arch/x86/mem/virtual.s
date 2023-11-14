global ArchGetVirtFaultAddress
global x86SetCr3

ArchGetVirtFaultAddress:
    mov eax, cr2
    ret

x86SetCr3:
    mov eax, [esp + 4]
    mov cr3, eax
    ret