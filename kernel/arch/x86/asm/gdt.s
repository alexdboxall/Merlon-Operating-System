
;
; x86/asm/gdt.s - Global Descriptor Table
;
; TODO: ...
;

global x86LoadGdt
x86LoadGdt:
	mov eax, [esp + 4]
	lgdt [eax]
	jmp 0x08:.reload

.reload:
	mov ax, 0x10
	mov ds, ax
	mov es, ax
	mov ss, ax
	ret
