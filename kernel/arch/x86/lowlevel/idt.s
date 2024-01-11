
global x86LoadIdt
x86LoadIdt:
	mov eax, [esp + 4]
	lidt [eax]
	ret