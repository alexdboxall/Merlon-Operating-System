
;
; This file is quite the circus. But that's alright (for now at least) - the
; whole point of splitting up the bootloader into a generic C part and a
; platform specific helper file is to ensure that all the 'clowning around' only
; needs to be done one per platform, and then it can just be hidden away behind
; the curtain.
;

; 0x500  - 0x5FF		INITIAL RAM MAP (BIOS FORMAT)
; 0x900  - 0x93F		BOOTSTRAP GDT
; 0x6F?? - 0x7000		REAL MODE (FIRMWARE.LIB STACK)
; 0x7C00 - 0x7DFF		STAGE 1/2 BOOTLOADER CODE/DATA
; 0x8000 - 0xBFFF		BOOTLOAD.EXE CODE/DATA
; 0xC000 - 0xCFFF		FIRMWARE.LIB CODE/DATA
; 0xD000 - 0xDFFF		FIRMWARE.LIB SECTOR BUFFER
; 0xF000 - 0xFFFF		ALL (PRE-KERNEL) STACKS

org 0xC000
bits 16

start:
	mov ax, 0
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax
	mov sp, ax

	mov [boot_drive], dl

	; Disable blink
	mov ax, 0x1003
	xor bx, bx
	int 0x10

	call generate_memory_map

	; Load a GDT
	xor eax, eax
	mov bx, 0xFFFF

	mov [fs:0x900], eax
	mov [fs:0x904], eax

	;32 bit p-mode code
	mov [fs:0x908], word bx
	mov [fs:0x90A], word ax
	mov [fs:0x90C], word 0x9A00
	mov [fs:0x90E], word 0xCF
	
	;32 bit p-mode code
	mov [fs:0x910], word bx
	mov [fs:0x912], word ax
	mov [fs:0x914], word 0x9200
	mov [fs:0x916], word 0xCF
	
	;16 bit p-mode code
	mov [fs:0x918], word bx
	mov [fs:0x91A], word ax
	mov [fs:0x91C], word 0x9A00
	mov [fs:0x91E], word 0x0F

	;16 bit p-mode data
	mov [fs:0x920], word bx
	mov [fs:0x922], word ax
	mov [fs:0x924], word 0x9200
	mov [fs:0x926], word 0x0F

	mov [fs:0x928], word 0x27
	mov [fs:0x92A], dword 0x900

	mov eax, 0x928
	lgdt [eax]        ; load GDT into GDTR

    mov eax, cr0	
    or eax, 1
    mov cr0, eax

    jmp 0x8:protected_mode_main

	hlt
	jmp $

boot_drive db 0


; The number of entries will be stored at 0x500, the map will be put at 0x504
; From here:
;		https://wiki.osdev.org/Detecting_Memory_(x86)
generate_memory_map:
    mov di, 0x504          ; Set di to 0x8004. Otherwise this code will get stuck in `int 0x15` after some entries are fetched 
	xor ebx, ebx		; ebx must be 0 to start
	xor bp, bp		; keep an entry count in bp
	mov edx, 0x0534D4150	; Place "SMAP" into edx
	mov eax, 0xe820
	mov [es:di + 20], dword 1	; force a valid ACPI 3.X entry
	mov ecx, 24		; ask for 24 bytes
	int 0x15
	;jc short .failed	; carry set on first call means "unsupported function"
	;mov edx, 0x0534D4150	; Some BIOSes apparently trash this register?
	;cmp eax, edx		; on success, eax must have been reset to "SMAP"
	;jne short .failed
	;test ebx, ebx		; ebx = 0 implies list is only 1 entry long (worthless)
	;je short .failed
	jmp short .jmpin
.e820lp:
	mov eax, 0xe820		; eax, ecx get trashed on every int 0x15 call
	mov [es:di + 20], dword 1	; force a valid ACPI 3.X entry
	mov ecx, 24		; ask for 24 bytes again
	int 0x15
	jc short .e820f		; carry set means "end of list already reached"
	mov edx, 0x0534D4150	; repair potentially trashed register
.jmpin:
	jcxz .skipent		; skip any 0 length entries
	cmp cl, 20		; got a 24 byte ACPI 3.X response?
	jbe short .notext
	test byte [es:di + 20], 1	; if so: is the "ignore this data" bit clear?
	je short .skipent
.notext:
	mov ecx, [es:di + 8]	; get lower uint32_t of memory region length
	or ecx, [es:di + 12]	; "or" it with upper uint32_t to test for zero
	jz .skipent		; if length uint64_t is 0, skip entry
	inc bp			; got a good entry: ++count, move to next storage spot
	add di, 24
.skipent:
	test ebx, ebx		; if ebx resets to 0, list is complete
	jne short .e820lp
.e820f:
	mov [0x500], bp	; store the entry count
.failed:
	ret


enableA20:
	cli
	pusha

    call    wait_input
    mov     al,0xAD
    out     0x64,al		; disable keyboard
    call    wait_input

    mov     al,0xD0
    out     0x64,al		; tell controller to read output port
    call    wait_output

    in      al,0x60
    push    eax		; get output port data and store it
    call    wait_input

    mov     al,0xD1
    out     0x64,al		; tell controller to write output port
    call    wait_input

    pop     eax
    or      al,2		; set bit 1 (enable a20)
    out     0x60,al		; write out data back to the output port

    call    wait_input
    mov     al,0xAE		; enable keyboard
    out     0x64,al

    call    wait_input
popa
    ret

wait_input:
    in      al,0x64
    test    al,2
    jnz     wait_input
    ret

wait_output:
    in      al,0x64
    test    al,1
    jz      wait_output
    ret

bits 32

protected_mode_main:
	cli
	mov	ax, 0x10
	mov	ds, ax
	mov	ss, ax
	mov	es, ax
	mov esp, 0x10000
	
	mov cx, [0x500]
	mov [num_ram_entries], cx

	
	mov edi, proper_ram_table
	mov esi, 0x504
.ram_table_loop:
	; base address
	mov eax, [esi]
	mov [edi], eax
	mov eax, [esi + 4]
	mov [edi + 4], eax
	add edi, 8
	add esi, 8

	; length
	mov eax, [esi]
	mov [edi], eax
	mov eax, [esi + 4]
	mov [edi + 4], eax
	add edi, 8
	add esi, 8

	mov eax, [esi]
	cmp eax, 1
	je .goodram
	cmp eax, 2
	je .acpi

	mov eax, 2
	jmp .x

.goodram:
	mov eax, 0
	jmp .x

.acpi:
	mov eax, 1

.x:
	mov [edi], eax
	add edi, 8
	add esi, 8

	dec cx
	jnz .ram_table_loop

	push dword 0x8000
	push bootload_path
	call read_file
	add esp, 8
	push firmware_info
	call 0x8000

bootload_path db "System/bootload.exe", 0

firmware_info:
	num_ram_entries: dd 0
	dd proper_ram_table 
	dd 0x100000
	dd 0
	db "System/kernel.exe"
	times (32 - 17) db 0
	dd putchar
	dd check_key
	dd reboot
	dd sleep_100ms
	dd get_file_size
	dd read_file
	dd exit_firmware

putchar:
	mov eax, [esp + 4]	; x
	mov ebx, [esp + 8]	; y
	mov ecx, [esp + 12]	; char
	mov edx, [esp + 16]	; col
	
	shl ebx, 2			; y * 4
	add ebx, [esp + 8]	; y * 5
	shl ebx, 4			; y * 80
	add ebx, eax		; y * 80 + x
	add ebx, ebx		; 2 * (y * 80 + x)
	add ebx, 0xB8000
	mov [ebx], cl
	mov [ebx + 1], dl
	ret

check_key:
	mov [realModeCommand], byte 0
	call SWITCH_TO_REAL
	ret

reboot:
	jmp 0xFFFF:0
	ret

sleep_100ms:
	mov [realModeCommand], byte 1
	call SWITCH_TO_REAL
	ret

get_file_size:
	pusha
	mov esi, [esp + 4 + 8 * 4]
	call demofs_find_file
	mov ecx, eax
	or ecx, ebx
	jz fail_generic

	mov eax, [esp + 8 + 8 * 4]
	mov [eax], ebx
	popa
	xor eax, eax
	ret

read_file:
	pusha
	mov esi, [esp + 4 + 8 * 4]
	call demofs_find_file
	mov ecx, eax
	or ecx, ebx
	jz fail_generic

	add ebx, 511
	shr ebx, 9
	mov edi, [esp + 8 + 8 * 4]
.next_sector:
	push eax
	push ebx
	push edi
	call real_read_sector
	pop edi
	pop ebx
	pop eax

	mov ecx, 512
	mov esi, 0xD000
	cld
	rep movsb

	inc eax
	dec ebx
	jnz .next_sector

	popa
	xor eax, eax
	ret

fail_generic:
	popa
	mov eax, 1
	ret

exit_firmware:
	ret


; IN:
; 	ESI = path to file
; OUT:
; 	EAX = starting sector of file (both 0 on fail)
; 	EBX = length of file
demofs_find_file:
	cld
	mov edx, 9		; root inode

.next_segment:
	xor ecx, ecx
	xor ebx, ebx	; index into segment_buffer

	mov [segment_buffer], ecx
	mov [segment_buffer + 4], ecx
	mov [segment_buffer + 8], ecx
	mov [segment_buffer + 12], ecx
	mov [segment_buffer + 16], ecx
	mov [segment_buffer + 20], ecx

.get:
	lodsb

	cmp al, '/'
	je .perform_follow
	cmp al, 0
	je .perform_follow

	mov [segment_buffer + ebx], al
	inc ebx	
	jmp .get

.perform_follow:
	push esi
	mov eax, edx
	mov esi, segment_buffer
	call demofs_follow
	mov edx, ebx
	cmp eax, 0
	je .not_found
	cmp eax, 2
	je .continue
	; found a file (not a directory) - so must be end
	add esp, 4
	mov eax, ebx
	mov ebx, ecx
	ret

.continue:
	pop esi
	jmp .next_segment

.not_found:
	add esp, 4
	xor eax, eax
	xor ebx, ebx
	ret

segment_buffer_ptr db 0
segment_buffer:
	times 24 db 0

; IN:
; 	EAX = root inode
; 	ESI = 24 byte, null padded path component
; OUT:
; 	EAX = 2 if directory, 1 if file, 0 if not found
;	EBX = starting inode
; 	ECX = length if file
demofs_follow:
	cld

.next_sector:
	pusha
	call real_read_sector
	popa
	mov edi, 0xD020
	mov edx, 15
.next_within_sector:
	push edi
	push esi
	mov ecx, 24
	repe cmpsb
	je short .found
	pop esi
	pop edi
	add edi, 32
	dec edx
	jnz .next_within_sector

	; ok, need to move onto the next one...
	mov al, [0xD000]
	cmp al, 0xFE
	jne .not_found
	mov eax, [0xD000]
	shr eax, 8
	jmp .next_sector

.found:
	pop esi
	pop edi
	; the entry starts at EDI, and goes thru to EDI + 31 (incl.)
	mov ebx, [edi + 24 + 4]
	and ebx, 0xFFFFFF
	mov al, [edi + 24 + 7]
	and al, 1
	jz .is_file

	; directory
	mov eax, 2
	ret

.is_file:
	mov ecx, [edi + 24]
	mov eax, 1
	ret

.not_found:
	xor eax, eax
	ret

; EAX = LBA
; output at 0xD000
real_read_sector:
	mov [realModeCommand], byte 2
	mov [realModeData1], eax
	call SWITCH_TO_REAL
	ret

rmSwitchyStack dd 0

; 0 = read key
; 1 = wait 100ms
; 2 = read sector
realModeCommand db 0

realModeData1 dd 0
realModeData2 dd 0
realModeRet1 dd 0
realModeRet2 dd 0

SWITCH_TO_REAL:
	push edi
	push esi
	push ebp
	mov [realModeData1], eax
	mov [realModeData2], ebx

	cli ; 8.9.2. Step 1.

	mov [rmSwitchyStack], esp

	mov eax,cr0 ; 8.9.2. Step 2.
	and eax,0x7FFFFFFF	;0x7FFFFFFF
	mov cr0,eax

	jmp 0x18:prot16


[BITS 16]

prot16:
	mov ax,0x0020 ; 8.9.2. Step 4.
	mov ds,ax
	mov es,ax
	mov fs,ax
	mov gs,ax
	mov ss,ax

	mov sp, 0x7000

	mov eax,cr0 ; 8.9.2. Step 2.
	and al,0xFE	
	mov cr0,eax	;FREEZE!

	jmp word 0:real16		; 8.9.2. Step 7.

align 16
bits 16
real16:
	cli
	mov ax, 0
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax

	mov sp, 0x7000

	mov al, [realModeCommand]
	cmp al, 0
	je bios_read_key
	cmp al, 1
	je bios_wait_100ms
	cmp al, 2
	je bios_read_sector

	mov [realModeRet1], dword -1
	jmp goBackHome

bios_read_key:
	jmp goBackHome

bios_wait_100ms:
	mov ah, 0x86
	mov cx, 0x1
	mov dx, 34464		; 34,464 + 1 * 65536 = 100,000us = 100ms
	int 0x15
	jmp goBackHome

bios_read_sector:
	mov [biglba], dword 0
	mov eax, [realModeData1]
	mov [d_lba], eax

	mov [aaab], byte 0x10
	mov [aaac], byte 0x00
	mov [blkcnt], word 1
	mov [d_add], word 0
	mov [d_seg], word 0xD00
	mov dl, byte [boot_drive]
	mov ah, 0x42
	mov si, DAPACK
	int 0x13
	jnc goBackHome

	; do a 'non-extended read'
	; Get disk geometry
	mov ah, 0x8
	xor di, di			;guard against BIOS bugs
	mov es, di
	mov dl, byte [boot_drive]
	int 0x13
	jc short .readfail
	inc dh				;BIOS returns one less than actual value
	and cx, 0x3F		;NUM SECTORS PER CYLINDER IN CX
	mov bl, dh			
	xor bh, bh			;NUM HEADS IN BX
	lfs ax, [realModeData1]	;first load [d_lba] into GS:AX
	mov dx, fs			;then copy GS to DX to make it DX:AX
	div cx
	inc dl
	mov cl, dl
	xor dx, dx
	div bx
	and ah, 3
	shl ah, 6
	or cl, ah
						;SECTOR ALREADY IN CL
	mov ch, al			;CYL
	mov ax, 1			;SECTOR COUNT
	mov ah, 0x02		;FUNCTION NUMBER
	mov dh, dl			;HEAD
	mov dl, [boot_drive]
	mov bx, 0xD00
	mov es, bx
	xor bx, bx
	int 0x13
	jc short .readfail
	jmp goBackHome
.readfail:
	mov [realModeRet1], dword 1
	jmp goBackHome

align 8
DAPACK:
aaab	db	0x10
aaac	db	0
blkcnt:	dw	1		; int 13 resets this to # of blocks actually read/written
d_add:	dw	0x0000	; memory buffer destination address (0:7c00)
d_seg:	dw	0x0000	; in memory page zero
d_lba:	dd	0		; put the lba to read in this spot
biglba:	dd	0		; more storage bytes only for big lbas ( > 4 bytes )

goBackHome:
	cli

	mov [aaab], byte 0x10
	mov [aaac], byte 0
	mov [biglba], dword 0


	mov [realModeRet1], eax
	mov [realModeRet2], ebx

	xor ax, ax 
	mov ds, ax
	mov es, ax
	mov ss, ax
	mov fs, ax

	cli
	mov [fs:0x900], dword 0
	mov [fs:0x904], dword 0

	;32 bit p-mode code
	mov [fs:0x908], word 0xFFFF
	mov [fs:0x90A], word 0
	mov [fs:0x90C], word 0x9A00
	mov [fs:0x90E], word 0xCF
	
	;32 bit p-mode code
	mov [fs:0x910], word 0xFFFF
	mov [fs:0x912], word 0
	mov [fs:0x914], word 0x9200
	mov [fs:0x916], word 0xCF
	
	;16 bit p-mode code
	mov [fs:0x918], word 0xFFFF
	mov [fs:0x91A], word 0
	mov [fs:0x91C], word 0x9A00
	mov [fs:0x91E], word 0x0F

	;16 bit p-mode data
	mov [fs:0x920], word 0xFFFF
	mov [fs:0x922], word 0
	mov [fs:0x924], word 0x9200
	mov [fs:0x926], word 0x0F

	mov [fs:0x928], word 0x27
	mov [fs:0x92A], dword 0x900

	mov eax, 0x928
	lgdt [eax]        ; load GDT into GDTR

	mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    jmp .flush2
	.flush2:

	cli
	mov eax, cr0 
	or al, 1		; set PE (Protection Enable) bit in CR0 (Control Register 0)
	mov cr0, eax

	jmp 0x8:protected_mode_return ; + 0x7E00

BITS 32
protected_mode_return:
	cli

	mov	ax, 0x10
	mov	ds, ax
	mov	ss, ax
	mov	es, ax

	mov esp, [rmSwitchyStack]
	mov eax, [realModeRet1]
	mov ebx, [realModeRet2]

	pop ebp
	pop esi
	pop edi
	ret

proper_ram_table:
