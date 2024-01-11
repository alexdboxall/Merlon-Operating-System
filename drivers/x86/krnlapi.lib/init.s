
global _krnlapi_start
extern loader_main

_krnlapi_start:
    ; The kernel leaves the pointer to some data we need in EDX 
    push edx
    call loader_main
    add esp, 4
    
    ; The program should normally have called exit() by this point, or left main() 
    ; (which in turn calls exit). However, if someone is e.g. not using the C library, we need
    ; to do something. We will just decide to crash the app immediately instead of letting it 
    ; wonder off into nowhere.

    hlt         ; will kill a ring 3 process
    jmp $
