
;
;
; x86/lowlevel/idt.s - Load IDT
;
; We need assembly in order to access the LIDT instruction so we can load
; the IDT. Hence we define a function in assembly to load the IDT for us.
;
;

global x86LoadIdt
x86LoadIdt:
	; The address of the IDTR is passed in as an argument
	mov eax, [esp + 4]
	lidt [eax]

	ret