
org 0x7C00
bits 16

jmp short start
nop

kernel_kilobytes dw 0x0
boot_drive_number db 0

start:
	cli
	cld
	jmp 0:set_cs

set_cs:
	xor ax, ax
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax

	; Set the stack to 0x0000:0x0000, so it will wrap around to
	; 0x0000:0xFFFE and so on.
	mov ss, ax
	mov sp, ax

	mov [boot_drive_number], dl

	; Clear the screen by calling the BIOS.
	mov ax, 0x3
	int 0x10
	
	; Load the rest of the bootloader, and the DemoFS table.
	xor eax, eax
	inc ax
	mov cx, 8
	mov di, 0x7C00 + 512
	call read_sector

	; Read the kernel's inode number, and size.
	mov eax, [0x7C00 + 512 * 8 + 12]
	mov ecx, eax
	
	; Inode number
	and eax, 0xFFFFFF
	
	; Size in kilobytes (must do two shifts to chop off the low bit,
	; or I guess you could do shr ebx, 23; and bl, 0xFE)
	shr ecx, 24
	shl ecx, 1
	mov [kernel_kilobytes], cx
	
	; Load a sector at at time to 0x1000:0x0000
	; ie. 0x10000. This allows sizes of approx. 448KB (our filesystem
	; only supports a kernel size of 255KB anyway).
	mov bx, 0x1000
	mov gs, bx
	xor di, di	

next_sector:
	push cx
	mov cx, 1
	call read_sector
	pop cx

	; Move to next sector
	inc eax
	
	; Move 512 bytes along ( = 0x200 bytes = offset change of 0x20)
	mov bx, gs
	add bx, 0x20
	mov gs, bx
	loop next_sector

	jmp start_main


preferred_modes_table:
	db 0x3F
	db 0x18
	db 0x1A
	db 0x14
	;db 0x38
	db 0x17
	db 0x16
	db 0x15
	;db 0x2E
	db 0x13
	db 0x05
	db 0x03
	db 0x12
	;db 0x29
	db 0x11
	db 0x10
	db 0x01
	

read_sector:
	; Put sector number in EAX. Put the number of sectors in CX.
	; Put the destination offset in DI. The segment will be GS.

	pushad
	push es

	; This attempts to read from an 'extended' hard drive (i.e. all
	; modern drives)
	mov [io_lba], eax
	mov ax, gs
	mov [io_segment], ax
	mov dl, byte [boot_drive_number]
	mov ah, 0x42
	mov si, disk_io_packet
	mov [disk_io_packet], byte 0x10
	mov [io_offset], di
	mov [io_count], cx
	int 0x13
	
	; If succeeded, we can exit.
	jnc skip_floppy_read
	
	; Otherwise we must call the older CHS method of reading (likely
	; from a floppy).

	mov ah, 0x8
	xor di, di			;guard against BIOS bugs
	mov es, di
	mov dl, byte [boot_drive_number]
	int 0x13
	jc short $

	inc dh				;BIOS returns one less than actual value

	and cx, 0x3F		;NUM SECTORS PER CYLINDER IN CX

	mov bl, dh			
	xor bh, bh			;NUM HEADS IN BX
	
	lfs ax, [io_lba]	;first load [d_lba] into GS:AX
	mov dx, fs			;then copy GS to DX to make it DX:AX

	div cx

	inc dl
	mov cl, dl

	xor dx, dx
	div bx

	;LBA					0x2000
	;ABSOLUTE SECTOR:	CL	0x03
	;ABSOLUTE HEAD:		DL	0x0A
	;ABSOLUTE CYLINDER:	AX	0x08

	;get the low two bits of AH into the top 2 bits of CL
	and ah, 3
	shl ah, 6
	or cl, ah
						;SECTOR ALREADY IN CL
	mov ch, al			;CYL
	mov ax, [io_count]	;SECTOR COUNT
	mov ah, 0x02		;FUNCTION NUMBER
	mov dh, dl			;HEAD
	mov dl, [boot_drive_number]
	mov bx, [io_segment]
	mov es, bx
	mov bx, [io_offset]
	int 0x13
	jc short $

skip_floppy_read:
	pop es
	popad

	ret

	
times 0x1BE - ($-$$) db 0

; Partition table
db 0x80					; bootable
db 0x01
db 0x01
db 0x01
db 0x0C					; pretend to be FAT32
db 0x01
db 0x01
db 0x01
dd 1					; start sector (we put a dummy VBR here)
dd 131072 * 16			; total sectors in partition

; A data packet we use to interface with the BIOS extended disk functions.
; We'll borrow the memory from the partition table
align 16
disk_io_packet:
	db 0x00
	db 0x00
io_count:
	dw 0x0000
io_offset:
	dw 0x0000
io_segment:
	dw 0x0000
io_lba:
	dd 0
	dd 0

times 0x1FE - ($-$$) db 0
dw 0xAA55

; Pretend to be a FAT32 partition by having a 'valid enough' VBR
; (this will never get used, it just might 'help' with the USB)

bits 16
jmp short vbr_dummy
nop
db "DUMMYVBR"
dw 0x200
db 0x1
dw 0x187E
db 0x2
dw 0x0
dw 0x0
db 0xF8
dw 0x0
dw 0x0
dw 0x0
dd 0x1
dd 131072
dd 0x3C1
dw 0
dw 0
dd 2
dw 1
dw 6
db "DUMMY DUMMY "
db 0x80
db 0x00
db 0x28
dd 0xDEADC0DE

vbr_dummy:
	jmp 0x7C00

times (512 * 2 - 2) - ($-$$) db 0x90
dw 0xAA55


start_main:
	mov si, sysinfo_header
	call write_text
	mov si, blank
	call write_text
	call write_text
	call write_text
	call write_text
	call write_text
	call write_text
	call write_text

	mov ax, 0x0100

.next:
	mov dx, [cur_y]
	call show_vesa_info
	mov [cur_y], dx
	call wait_key
	inc ax
	jmp .next


show_vesa_info:
	pusha
	mov si, ax
	call write_hex
	mov si, blank
	call write_text
	call write_text
	call write_text

    mov cx, ax
	mov ax, 0x100 
    mov es, ax
    mov ax, 0x4F01
    xor di, di
	push es
    int 0x10
	pop es

	test ah, ah
	jnz .bad_mode

	mov si, str_attributes
	call write_text
	mov si, [es:0]
	call write_hex
	mov si, blank
	call write_text
	call write_text

	mov si, str_winfuncptr
	call write_text
	mov si, [es:12]
	call write_hex
	mov si, [es:14]
	call write_hex
	mov si, blank
	call write_text

	mov si, str_framebuffer
	call write_text
	mov si, [es:40]
	call write_hex
	mov si, [es:42]
	call write_hex
	mov si, blank
	call write_text

	mov si, str_pitch
	call write_text
	mov si, [es:16]
	call write_hex

	mov si, str_width
	call write_text
	mov si, [es:18]
	call write_hex

	mov si, str_height
	call write_text
	mov si, [es:20]
	call write_hex

	mov si, str_bpp
	call write_text
	xor ax, ax
	mov al, [es:25]
	mov si, ax
	call write_hex

	mov si, str_banks
	call write_text
	xor ax, ax
	mov al, [es:26]
	mov si, ax
	call write_hex

	mov si, str_banksize
	call write_text
	xor ax, ax
	mov al, [es:28]
	mov si, ax
	call write_hex

	mov si, blank
	call write_text
	call write_text
	call write_text
	call write_text
	
	popa
	ret

.bad_mode:
	mov si, bad_mode
	call write_text
	mov si, blank
	mov cx, 31
.keep_blanking:
	call write_text
	loop .keep_blanking
	popa
	ret

str_framebuffer db "Framebuffer:", 0
str_winfuncptr db "Win func ptr:", 0
str_pitch db "Pitch:", 0
str_width db "Width:", 0
str_height db "Height:", 0
str_bpp db "Bits per pixel:", 0
str_banks db "No. banks:", 0
str_banksize db "Bank size KB:", 0
str_attributes db "Attributes:       ", 0
blank 		   db "                  ", 0
bad_mode 	   db "Mode not supported", 0
sysinfo_header db "VESA Info", 0

wait_key:
	pusha
	mov ah, 0x10
	int 0x16
	popa
	ret

write_text:
	pusha
	push es
	mov ax, 0xB800
	mov es, ax

	xor di, di
	mov di, [cur_y]
	mov dx, di
.restart:
	lodsb
	or al, al
	jz .finish

	mov [es:di], al
    inc di
    mov [es:di], byte 0x07
    inc di
	jmp .restart


.finish:
	add dx, 40
    mov [cur_y], dx
	pop es
	popa
	ret

write_hex:
    pusha
	push es
	mov ax, 0xB800
	mov es, ax

    mov bx, hex_chars

	xor di, di
    mov di, [cur_y]
	mov dx, di

    mov cx, 4
.restart:
    rol si, 4
    mov ax, si
    and al, 0xF
    xlat

    mov [es:di], al
    inc di
    mov [es:di], byte 0x07
    inc di
    
    loop .restart

	add dx, 40
    mov [cur_y], dx

	pop es
    popa
    ret

cur_y dw 320
hex_chars db "0123456789ABCDEF"

times (512 * 8) - ($-$$) db 0