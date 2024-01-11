#include <cpu.h>
#include <arch.h>
#include <assert.h>
#include <heap.h>
#include <string.h>
#include <spinlock.h>
#include <debug.h>
#include <irql.h>

static struct cpu cpu_table[ARCH_MAX_CPU_ALLOWED];
static int num_cpus_running = 1;

static void InitCpuTableEntry(int index) {
    /*
     * The boot CPU can't use dynamic memory, as this happens before we have a heap.
     */
    static platform_cpu_data_t boot_cpu_data;

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
 */
void InitCpuTable(void) {
    assert(num_cpus_running == 1);
    InitCpuTableEntry(0);
}

/*
 * Performs platform-specific initialisation of the bootstrap CPU (e.g. initialising interrupts,
 * system timers, segments, etc.).
 */
void InitBootstrapCpu(void) {
    EXACT_IRQL(IRQL_STANDARD);
    ArchInitBootstrapCpu(cpu_table);
    MarkTfwStartPoint(TFW_SP_AFTER_BOOTSTRAP_CPU);
}

void InitOtherCpu(void) {
    /*
     * We initialise the next entry, even before we know there's a CPU there. If there isn't a 
     * CPU there, we don't increment `num_cpus_running` so the entry won't get used.
     */
    InitCpuTableEntry(1);

    while (ArchInitNextCpu(cpu_table + num_cpus_running)) {
        ++num_cpus_running;
        InitCpuTableEntry(num_cpus_running);
    }

    MarkTfwStartPoint(TFW_SP_AFTER_ALL_CPU);
}

int GetCpuCount(void) {
    return num_cpus_running;
}

struct cpu* GetCpuAtIndex(int index) {
    assert(index >= 0 && index < GetCpuCount());
    return cpu_table + index;
}

struct cpu* GetCpu(void) {
    return cpu_table + ArchGetCurrentCpuIndex();
}
