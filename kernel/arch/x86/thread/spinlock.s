
;
;
; x86/thread/spinlock.s - Spinlocks
;
; Implement spinlocks in assembly so we can guarentee that they are
; atmoic.
;
;


global ArchSpinlockAcquire
global ArchSpinlockRelease

ArchSpinlockAcquire:
	; The address of the lock is passed in as an argument
	mov eax, [esp + 4]

.try_acquire:
	; Try to acquire the lock
	lock bts dword [eax], 0
	jc .spin_wait

	ret

.spin_wait:
	; Lock was not acquired, so do the 'spin' part of spinlock

	; Hint to the CPU that we are spinning
	pause

	; No point trying to acquire it until it is free
	test dword [eax], 1
	jnz .spin_wait
	
	; Now that it is free, we can attempt to atomically acquire it again
	jmp .try_acquire


ArchSpinlockRelease:
	; The address of the lock is passed in as an argument
	mov eax, [esp + 4]
	lock btr dword [eax], 0
	ret
