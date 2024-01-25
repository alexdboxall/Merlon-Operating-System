
;
; x86/asm/misc.s - Miscellaneous Assembly
;
; Provides miscellaneous functions that must be implemented in assembly due to
; the fact they need to use platform specific instructions.
;

global ArchGetCurrentCpuIndex
global ArchReadTimestamp
global ArchEnableInterrupts
global ArchDisableInterrupts
global ArchStallProcessor

ArchGetCurrentCpuIndex:
    ; Needs to be something that can't be modified by user code (e.g. a debug 
	; register).
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
