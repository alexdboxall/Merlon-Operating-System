
global x86LoadIdt
x86LoadIdt:
	; The address of the IDTR is passed in as an argument
	mov eax, [esp + 4]
	lidt [eax]

	ret