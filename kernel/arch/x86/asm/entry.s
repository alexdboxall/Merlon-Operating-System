
;
; x86/asm/entry.s - Kernel Entry Point
;
; Provides the entry point to the kernel '_start', which gets jumped to by the
; bootloader.
;
	
; The initial kernel stack.
section .bss
align 16
stack_bottom:
resb 4 * 1024
stack_top:

; We need temporary bootstrap paging structures so we can get the kernel into
; high memory. With the below, we can map 4MB of memory, so the kernel can be 
; 3MB at most as it gets loaded at 1MB). We will replace and free these paging
; structures later in the kernel proper. 
align 4096
global boot_page_directory
global boot_page_table1
boot_page_directory: resb 4096
boot_page_table1: resb 4096

; The start of the kernel itself - this will be called by the bootloader.
; This is loaded at 1MB, not at 0xC0000000 + 1MB, so it must go in a special
; section. Information from the bootloader is in EBX, so we must preserve it!
section .lowram.text
extern KernelMain
extern _kernel_end
global _start
_start:
	cli
	cld	

	; Bootloader info table
	mov ebx, [esp + 4]
	add ebx, 0xC0000000
	
    ; Work out how many pages in the first 4MB need to be mapped
    ; (we map the low 1MB, and then the kernel)
    mov ecx, _kernel_end
    add ecx, 0xFFF
    and ecx, 0x0FFFF000
    shr ecx, 12
    mov eax, 1024
    sub eax, ecx
    
	; Set up a loop to fill in the page table.
	mov edi, boot_page_table1 - 0xC0000000
	xor esi, esi
	mov ecx, 1024

.mapNextPage:
	mov edx, esi
	or edx, 3			; make the page present and writable
    cmp ecx, eax
    jg .keep            ; ** remember, the loop counter is going down **
    xor edx, edx        ; not present
.keep:
	mov [edi], edx

.incrementPage:
	add esi, 4096
	add edi, 4
	loop .mapNextPage

.endMapping:
	; Identity map and put the mappings at 0xC0000000. This way we won't page 
	; fault before we jump over to the kernel in high memory (we're at 1MB)
	mov [boot_page_directory - 0xC0000000 + 0], dword boot_page_table1 - 0xC0000000 + 3 + 256
	mov [boot_page_directory - 0xC0000000 + 768 * 4], dword boot_page_table1 - 0xC0000000 + 3 + 256

	; Set the page directory
	mov ecx, boot_page_directory - 0xC0000000
	mov cr3, ecx

	; Enable paging
	mov ecx, cr0
	or ecx, (1 << 31)
	or ecx, (1 << 16)		; enforce read-only pages in ring 0
	mov cr0, ecx

	lea ecx, KernelEntryPoint
	jmp ecx
.end:

section .data

global vesa_pitch
global vesa_width
global vesa_height
global vesa_depth
global vesa_framebuffer

vesa_depth db 0
vesa_framebuffer dd 0
vesa_width dw 0
vesa_height dw 0
vesa_pitch dw 0

section .text

; The proper entry point of the kernel. Assumes the kernel is mapped into memory
; at 0xC0100000.
KernelEntryPoint:
	; Remove the identity paging and flush the TLB so the changes take effect
	mov [boot_page_directory], dword 0
	mov ecx, cr3
	mov cr3, ecx
	
	; On x86, we'll store the current CPU number in the DR3 register (so user 
	; code cannot modify it). Set it correctly now.
	xor eax, eax
	mov dr3, eax

	; Set the stack to the one we defined
	mov esp, stack_top

	; Call the kernel main function, passing it in the table given to us by the
	; bootloader.
	push ebx
	call KernelMain

	; We should never get here, but halt just in case
	cli
	hlt
	jmp $
