
;
;
; x86/lowlevel/misc.s - Miscellaneous Functions
;
; Serveral Arch_ functions map directly to assembly instructions, so
; implement them here.
;

global ArchReadTimestamp
global ArchEnableInterrupts
global ArchDisableInterrupts
global ArchStallProcessor
global ArchFlushTlb
global x86GetCr2
global x86AreCpusOn
global ArchGetCurrentCpuIndex

ArchGetCurrentCpuIndex:
	xor eax, eax
	mov ax, fs
	ret

ArchReadTimestamp:
	rdtsc
	ret
	
ArchEnableInterrupts:
	sti
	ret
	
ArchDisableInterrupts:
	cli
	ret
	
ArchStallProcessor:
	hlt
	ret
	
x86AreCpusOn:
    pushf
    pop eax
    and eax, 0x200
    shr eax, 9
    ret