
global setjmp
global longjmp

;int setjmp(size_t* env);
;void longjmp(size_t* env, int val);

; JMP_BUF LAYOUT
;   
;  0:    EBX
;  4:    ECX
;  8:    <unused>
; 12:    ESI
; 16:    EDI
; 20:    EBP
; 24:    EFLAGS
; 28:    EIP
; 32:    ESP
; 36:    <temporary data>

setjmp:
    mov eax, [esp + 4]          ; this is the jmp_buf buffer

    mov [eax +  0], ebx
    mov [eax +  4], ecx          
    mov [eax + 12], esi          
    mov [eax + 16], edi          
    mov [eax + 20], ebp  

    pushf 
    pop dword [eax + 24]        ; save eflags       

    pop edx                     ; get the return value for setjmp
    mov [eax + 28], edx

    mov [eax + 32], esp
    
    ; return 0
    xor eax, eax 
    jmp edx


longjmp:
    mov edx, [esp + 4]          ; this is the jmp_buf buffer
    mov eax, [esp + 8]          ; this is the return value

    test eax, eax               ; check if return value is 0, and set to 1 if so (C standard says to do so)
    jnz .skip_inc
    inc eax
.skip_inc:

    mov ebx, [edx +  0]
    mov ecx, [edx +  4]
    mov esi, [edx + 12]
    mov edi, [edx + 16]
    mov ebp, [edx + 20]
    
    push dword [edx + 24]
    popf

    mov esp, [edx + 32]
    add esp, 4                  ; skip past the old return address (which we don't need, as 'longjmp' doesn't return there)
    jmp dword [edx + 28]        ; 'return'
