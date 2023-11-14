#pragma once

/*
* arch.h - Architecture-specific wrappers
*
* 
* Functions relating to hardware devices that must be implemented by
* any platform supporting the operating system.
* 
*/

/*
* config.h needs to define the following:
*	- ARCH_PAGE_SIZE
*	- either ARCH_STACK_GROWS_DOWNWARD or ARCH_STACK_GROWS_UPWARD
*	- ARCH_MAX_CPU_ALLOWED
*	- ARCH_MAX_RAM_KBS
* 	- the valid user area, via ARCH_USER_AREA_BASE and ARCH_USER_AREA_LIMIT
* 	- the valid kernel area, via ARCH_KRNL_SBRK_BASE and ARCH_KRNL_SBRK_LIMIT
*    		(the kernel and user areas must not overlap, but ARCH_USER_AREA_LIMIT may equal ARCH_KRNL_SBRK_BASE
 			 or ARCH_KRNL_SBRK_LIMIT may equal ARCH_USER_AREA_BASE)
*	- the user stack area, via ARCH_USER_STACK_BASE and ARCH_USER_STACK_LIMIT
*       	(may overlap with ARCH_USER_AREA_BASE and ARCH_USER_AREA_LIMIT)
*	- should provide a typedef for platform_cpu_data_t
*/


#include <machine/config.h>

#if ARCH_USER_STACK_BASE < ARCH_USER_AREA_BASE
#error "ARCH_USER_STACK_BASE must be greater than or equal to ARCH_USER_AREA_BASE"
#elif ARCH_USER_STACK_LIMIT > ARCH_USER_AREA_LIMIT
#error "ARCH_USER_STACK_LIMIT must be less than or equal to ARCH_USER_AREA_LIMIT"
#endif

#include <common.h>

struct arch_memory_range
{
	size_t start;
	size_t length;
};

struct vas;
struct vas_entry;
struct thread;
struct cpu;

struct arch_driver_t;

/*
* Needs to setup any CPU specific structures, set up virtual memory for the system
* and get the current_cpu structure sorted out.
*/
void ArchInitialiseBootstrapCpu(void); 

/*
* Called once after all CPUs are initialised.
*/
void ArchCompletedCpuInitialisation(void);

void ArchInitDevicesNoFs(void);
void ArchInitDevicesWithFs(void);

/*
* Only to be called in very specific places, e.g. turning interrupts
* on for the first time, the panic handler.
*/
void ArchEnableInterrupts(void);
void ArchDisableInterrupts(void);

/*
* Do nothing until (maybe) the next interrupt. If this is not supported by the
* system it may just return without doing anything.
*/
void ArchStallProcessor(void);

void ArchReboot(void);

void ArchIrqSpinlockAcquire(volatile size_t* lock);
void ArchIrqSpinlockRelease(volatile size_t* lock);

/*
* To be called repeatedly until it returns NULL. Each time will return a new memory
* range. An address of a static local object is permitted to be returned. 
* 
* NULL is returned if there is no more memory. No more calls to this function
* will be made after a NULL is returned.
*/
struct arch_memory_range* ArchGetMemory() warn_unused;

uint64_t ArchReadTimestamp(void);

void ArchFlushTlb(struct vas* vas);
void ArchAddMapping(struct vas* vas, struct vas_entry* entry);
void ArchUpdateMapping(struct vas* vas, struct vas_entry* entry);
void ArchUnmap(struct vas* vas, struct vas_entry* entry);
void ArchSetVas(struct vas* vas);
size_t ArchGetVirtFaultAddress(void* fault_info);
int ArchGetVirtFaultType(void* fault_info);
void ArchAddGlobalsToVas(struct vas* vas);
int ArchGetCurrentCpuIndex(void);

void ArchInitBootstrapCpu(struct cpu* cpu);

/*
 * If possible, initialises the next CPU, and returns true. If there are no more CPUs
 * to initialise, returns false. 
 */
bool ArchInitNextCpu(struct cpu* cpu);

void ArchInitVirt(void);