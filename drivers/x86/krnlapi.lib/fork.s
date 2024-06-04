
global fork_return_trampoline

fork_return_trampoline:
    ; Set return value to zero to indicate this is the child
    xor eax, eax

    ; This matches the kernel's task switch routine in `arch/x86/asm/switch.s` 
	pop ebp
	pop edi
	pop esi
	pop ebx
    
	ret