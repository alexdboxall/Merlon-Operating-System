

global x86LoadGdt
x86LoadGdt:
	mov eax, [esp + 4]
	lgdt [eax]
	jmp 0x08:.reloadSegments

.reloadSegments:
	mov ax, 0x10
	mov ds, ax
	mov es, ax
	mov ss, ax
	ret