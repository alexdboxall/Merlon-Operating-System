#pragma once

/*
 * As this is for x86 (not x86-64), we set the limit to 4GB. On x86-64, we can set
 * it larger. This will make the bitmap much larger, but this is no problem on an
 * x86-64 system (only ancient x86 systems will have e.g. 4MB of RAM).
 */
#define ARCH_MAX_RAM_KBS (1024 * 1024 * 4)

#define ARCH_PAGE_SIZE	        4096
#undef ARCH_STACK_GROWS_UPWARD 
#define ARCH_STACK_GROWS_DOWNWARD

/*
* Non-inclusive of ARCH_USER_AREA_LIMIT
*/
#define ARCH_USER_AREA_BASE     0x08000000
#define ARCH_USER_AREA_LIMIT    0xC0000000


#define ARCH_USER_STACK_BASE    0x08000000
#define ARCH_USER_STACK_LIMIT   0x10000000


/*
* Non-inclusive of ARCH_KRNL_SBRK_LIMIT. Note that we can't use the top 8MB,
* as we use that for recursive mapping.
*/
#define ARCH_KRNL_SBRK_BASE     0xC8000000
#define ARCH_KRNL_SBRK_LIMIT    0xFF800000

#define ARCH_MAX_CPU_ALLOWED    16


#include <machine/gdt.h>
#include <machine/idt.h>
#include <machine/tss.h>
#include <machine/regs.h>

typedef struct {
    /* Plz keep tss at the top, thread switching assembly needs it */
    struct tss* tss;

    struct gdt_entry gdt[16];
    struct idt_entry idt[256];

    struct gdt_ptr gdtr;
    struct idt_ptr idtr;

} platform_cpu_data_t;

typedef struct x86_regs platform_irq_context_t;