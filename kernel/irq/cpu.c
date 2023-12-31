#include <cpu.h>
#include <arch.h>
#include <assert.h>
#include <heap.h>
#include <string.h>
#include <spinlock.h>
#include <irql.h>

/*
 * Each CPU gets to store its own data (e.g. current VAS, current thread). Each CPU gets an index
 * (bootstrap CPU gets index 0) into this array. The CPU index is stored in some platform-dependent
 * way, and the index for the CPU executing it with ArchGetCurrentCpuIndex().
 */
static struct cpu cpu_table[ARCH_MAX_CPU_ALLOWED];

/*
 * This would normally be dynamically allocated, but we can't do that for the bootstrap CPU as it needs
 * to be done before virtual memory, and the heap, exists. Therefore, we must store it as a static global.
 */
static platform_cpu_data_t boot_cpu_data;

/*
 * The number of CPUs which have been initialised so far. The bootstrap CPU is treated as 'already
 * initalised' as it is already running.
 */
static int num_cpus = 1;

/*
 * Given a CPU index, initialises its index in the `cpu_table`. The bootstrap CPU (index 0) is treated specially,
 * as it uses the statically allocated `boot_cpu_data` instead of the heap for it's platform specific data.
 */
static void InitCpuTableEntry(int index) {
    cpu_table[index].cpu_number = index;
    cpu_table[index].platform_specific = index != 0 ? AllocHeapZero(sizeof(platform_cpu_data_t)) : &boot_cpu_data;
    cpu_table[index].irql = IRQL_STANDARD;
    cpu_table[index].global_vas_mappings = NULL;
    cpu_table[index].current_vas = NULL;
    cpu_table[index].current_thread = NULL;
    cpu_table[index].init_irql_done = false;
    cpu_table[index].postponed_task_switch = false;
    InitSpinlock(&cpu_table[index].global_mappings_lock, "glbl vas map", IRQL_SCHEDULER);
}

/*
 * Initialises the CPU table (`cpu_table`) for the bootstrap processor. This does *not* do any
 * platform-specific initialisation, as that requires other features to be set up first.
 * 
 * This should pretty much be the first thing to be initalised in the entire system, as it is
 * required for GetCpu(), GetThread(), GetVas(), GetIrql(), etc. to work.
 * 
 * IRQL is undefined before and during this call, but will be IRQL_STANDARD after the call.
 */
void InitCpuTable(void) {
    assert(num_cpus == 1);
    InitCpuTableEntry(0);
}

/*
 * Performs platform-specific initialisation of the bootstrap CPU (e.g. initialising interrupts,
 * system timers, segments, etc.).
 * 
 * @maxirql IRQL_STANDARD
 */
void InitBootstrapCpu(void) {
    EXACT_IRQL(IRQL_STANDARD);
    ArchInitBootstrapCpu(cpu_table);
}

void InitOtherCpu(void) {
    InitCpuTableEntry(1);

    while (ArchInitNextCpu(cpu_table + num_cpus)) {
        ++num_cpus;
        InitCpuTableEntry(num_cpus);
    }
}

int GetCpuCount(void) {
    return num_cpus;
}

struct cpu* GetCpuAtIndex(int index) {
    assert(index >= 0 && index < GetCpuCount());
    return cpu_table + index;
}

__attribute__((no_instrument_function)) struct cpu* GetCpu(void) {
    return cpu_table + ArchGetCurrentCpuIndex();
}

