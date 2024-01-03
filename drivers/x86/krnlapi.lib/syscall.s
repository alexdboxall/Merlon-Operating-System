
global _system_call

_system_call:
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