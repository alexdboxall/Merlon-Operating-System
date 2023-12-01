
global ArchReadTimestamp
global ArchEnableInterrupts
global ArchDisableInterrupts
global ArchStallProcessor
global ArchFlushTlb
global x86GetCr2
global x86AreCpusOn
global ArchGetCurrentCpuIndex

ArchGetCurrentCpuIndex:
        ; Needs to be something that can't be modified by user code (e.g. a debug register).
	mov eax, dr3
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
