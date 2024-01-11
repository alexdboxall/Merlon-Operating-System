
global execve_core

execve_core:
    mov eax, [esp + 4]          ; ENTRY POINT
    mov ebx, [esp + 8]          ; ARGC
    mov ecx, [esp + 12]         ; ARGV
    mov edx, [esp + 16]         ; ENVIRON
    mov edi, [esp + 20]         ; STARTING STACK
    mov esp, edi
    push edx
    push ecx
    push ebx
    call eax
    jmp $