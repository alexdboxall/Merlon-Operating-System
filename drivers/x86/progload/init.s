
global _start
global _SystemCall
extern ProgramLoader

_start:
    ; The kernel leaves the pointer to some data we need in EDX 
    push edx
    call ProgramLoader
    add esp, 4
    
    ; The program should normally have called exit() by this point, or left main() 
    ; (which in turn calls exit). However, if someone is e.g. not using the C library, we need
    ; to do something. We will just decide to crash the app immediately instead of letting it 
    ; wonder off into nowhere.

    hlt         ; will kill a ring 3 process
    jmp $


_SystemCall:
    push ebp
    mov ebp, esp
    
    push ebx
    push ecx
    push edx
    push esi
    push edi

    ; We saved EBP, so the pushes above don't affect us

    mov eax, [ebp + 8]          ; call number
    mov ebx, [ebp + 12]         ; arg 1
    mov ecx, [ebp + 16]         ; arg 2
    mov edx, [ebp + 20]         ; arg 3
    mov esi, [ebp + 24]         ; arg 4
    mov edi, [ebp + 28]         ; arg 5

    int 96

    pop edi
    pop esi
    pop edx
    pop ecx
    pop ebx

    pop ebp
    ret