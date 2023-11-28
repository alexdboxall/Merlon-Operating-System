global ArchPrepareStack
global ArchSwitchThread

extern ThreadInitialisationHandler
extern GetCpu

ArchPrepareStack:
	; We need to put 5 things on the stack - dummy values for EBX, ESI,
	; EDI and EBP, as well as the address of thread_startup_handler
	; 
	; This is because these get popped off in arch_switch_thread

	; Grab the address of the new thread's stack from our stack (it was
	; passed in as an argument)
	mov eax, [esp + 4]

	; We need to get to the bottom position, and we also need to return that
	; address in EAX so it can be put into the struct, so it makes sense to modify it.
	sub eax, 20

	; This is where the address of arch_switch_thread needs to go.
	; +0 is where EBP is, +4 is EDI, +8 for ESI, +12 for EBX,
	; and so +16 for the return value.
	; (see the start of arch_switch_thread for where these get pushed)
	mov [eax + 16], dword ThreadInitialisationHandler

	ret

ArchSwitchThread:		
	; The old and new threads are passed in on the stack as arguments, in that order.

	; The calling convention we use already saves EAX, ECX and EDX whenever a
	; function (e.g. ArchSwitchThread) is called. Therefore, we only need to save the other four.
	push ebx
	push esi
	push edi
	push ebp

	; We are now free to trash the general purpose registers (except ESP),
	; so we can now load the current task using the argument.

    ; First we have to save the old stack pointer. The old thread was the first
    ; argument, and we just pushed 4 things to the stack. The first argument gets
    ; pushed last, so read back 5 places. Also load the new thread's address in.

    mov edi, [esp + (4 + 1) * 4]        ; edi = old_thread
    mov esi, [esp + (4 + 2) * 4]        ; esi = new_thread

	; The second entry in a thread structure is guaranteed to be the stack pointer.
	; Save our stack there.
	mov [edi + 4], esp                  ; old_thread->stack_pointer = esp

    ; Now we can load the new thread's stack pointer.
    mov esp, [esi + 4]                  ; esp = new_thread->stack_pointer

    ; ESI is callee-saved, so no need to do anything here. We only need ESI and ESP
    ; at this point, so it's all good.

    call GetCpu                         ; eax = GetCpu()
	
	; The top of the kernel stack (which needs to go in the TSS for
	; user to kernel switches), is the first entry in new_thread.
	mov ebx, [esi]                      ; ebx = new_thread->kernel_stack_top
	
	; The third entry in current_cpu is a pointer to CPU specific data.
	mov ecx, [eax + 8]                  ; ecx = GetCpu()->platform_specific

	; The first entry in the CPU specific data is the TSS pointer
	mov edx, [ecx + 0]                  ; edx = GetCpu()->platform_specific->tss

	; Load the TSS's ESP0 with the new thread's stack
	mov [edx + 4], ebx

	; Now we have the new thread's stack, we can just pop off the state
	; that would have been pushed when it was switched out.
	pop ebp
	pop edi
	pop esi
	pop ebx

	ret