
;
; x86/asm/misc.s - Miscellaneous Assembly
;
; Provides miscellaneous functions that must be implemented in assembly due to
; the fact they need to use platform specific instructions.
;

global ArchGetCurrentCpuIndex
global ArchReadTimestamp
global ArchStallProcessor

ArchReadTimestamp:
	rdtsc
	ret
	
ArchStallProcessor:
	hlt
	ret
