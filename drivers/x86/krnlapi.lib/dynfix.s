
global dyn_fixup_asm
extern dyn_fixup

dyn_fixup_asm:
    call dyn_fixup
    add esp, 8
    jmp eax