
;
; x86/asm/spinlock.s - Spinlocks
;
; The mutual exclusion primative used by the kernel. Ensures that across CPUs, 
; that only one thread can hold the lock.
;

global ArchSpinlockAcquire
global ArchSpinlockRelease

ArchSpinlockAcquire:
	mov eax, [esp + 4]

.try_acquire:
	lock bts dword [eax], 0
	jc .spin_wait
	ret

.spin_wait:
	pause
	test dword [eax], 1
	jnz .spin_wait
	jmp .try_acquire


ArchSpinlockRelease:
	mov eax, [esp + 4]
	lock btr dword [eax], 0
	ret
