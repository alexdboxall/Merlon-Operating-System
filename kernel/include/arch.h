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
*	- ARCH_MAX_CPU_ALLOWED
*	- ARCH_MAX_RAM_KBS
*	- ARCH_BIG_ENDIAN or ARCH_LITTLE_ENDIAN
*	- the address in the kernel area, ARCH_PROG_LOADER_BASE, where the program loader lives, and
* 	- the valid user area, via ARCH_USER_AREA_BASE and ARCH_USER_AREA_LIMIT
* 	- the valid kernel area, via ARCH_KRNL_SBRK_BASE and ARCH_KRNL_SBRK_LIMIT
*    		(the kernel and user areas must not overlap, but ARCH_USER_AREA_LIMIT may equal ARCH_KRNL_SBRK_BASE
 			 or ARCH_KRNL_SBRK_LIMIT may equal ARCH_USER_AREA_BASE)
*	- the user stack area, via ARCH_USER_STACK_BASE and ARCH_USER_STACK_LIMIT
*       	(may overlap with ARCH_USER_AREA_BASE and ARCH_USER_AREA_LIMIT)
*	- a typedef for platform_cpu_data_t
*	- a typedef for platform_irq_context_t
*	- a typedef for platform_vas_data_t
*/


#include <machine/config.h>

#if ARCH_USER_STACK_BASE < ARCH_USER_AREA_BASE
#error "ARCH_USER_STACK_BASE must be greater than or equal to ARCH_USER_AREA_BASE"
#elif ARCH_USER_STACK_LIMIT > ARCH_USER_AREA_LIMIT
#error "ARCH_USER_STACK_LIMIT must be less than or equal to ARCH_USER_AREA_LIMIT"
#endif

#include <common.h>
#include <bootloader.h>

struct vas;
struct vas_entry;
struct thread;
struct file;
struct cpu;
struct rel_table;

struct arch_driver_t;

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

#define ARCH_POWER_STATE_REBOOT	1
#define ARCH_POWER_STATE_SHUTDOWN 2
#define ARCH_POWER_STATE_SLEEP 3
int ArchSetPowerState(int power_state);

void ArchSpinlockAcquire(volatile size_t* lock);
void ArchSpinlockRelease(volatile size_t* lock);

/*
* To be called repeatedly until it returns NULL. Each time will return a new memory
* range. An address of a static local object is permitted to be returned. 
* 
* NULL is returned if there is no more memory. No more calls to this function
* will be made after a NULL is returned.
*/
struct boot_memory_entry* ArchGetMemory(struct kernel_boot_info* boot_info) warn_unused;

uint64_t ArchReadTimestamp(void);

void ArchFlushTlb(struct vas* vas);
void ArchAddMapping(struct vas* vas, struct vas_entry* entry);
void ArchUpdateMapping(struct vas* vas, struct vas_entry* entry);
void ArchUnmap(struct vas* vas, struct vas_entry* entry);
void ArchSetVas(struct vas* vas);

void ArchGetPageUsageBits(struct vas* vas, struct vas_entry* entry, bool* accessed, bool* dirty);
void ArchSetPageUsageBits(struct vas* vas, struct vas_entry* entry, bool accessed, bool dirty);

// responsible for loading all symbols. should not close the file!
int ArchLoadProgramLoader(void* data, size_t* entry_point);
int ArchLoadDriver(size_t* relocation_point, struct file* file, struct rel_table** table, size_t* entry_point);
void ArchLoadSymbols(struct file* file, size_t adjust);
void ArchSwitchThread(struct thread* old, struct thread* new);
size_t ArchPrepareStack(size_t addr);

void ArchSwitchToUsermode(size_t entry_point, size_t user_stack, void* arg);

void ArchInitDev(bool fs);


/*
 * Used only if the AVL tree is insufficient, e.g. for deallocating part of the kernel region to, e.g.
 * reclaim the physical memory bitmap. Works only for the current VAS. Returns 0 on no mapping.
 */
size_t ArchVirtualToPhysical(size_t virtual);

/*
 * Initialises a given VAS with platform specific data (e.g. mapping the kernel in).
 */
void ArchInitVas(struct vas* vas);

/*
 * Initialises virtual memory in general, i.e. creates the first VAS.
 */
void ArchInitVirt(void);

int ArchGetCurrentCpuIndex(void);
void ArchSendEoi(int irq_num);
/*
 * Sets the CPUs interrupt state (and mask devices) based on an IRQL. This function
 * will always be called with interrupts completely disabled.
 */
void ArchSetIrql(int irql);

void ArchInitBootstrapCpu(struct cpu* cpu);

/*
 * If possible, initialises the next CPU, and returns true. If there are no more CPUs
 * to initialise, returns false. 
 */
bool ArchInitNextCpu(struct cpu* cpu);


