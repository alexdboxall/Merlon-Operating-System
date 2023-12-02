#include <cpu.h>
#include <arch.h>
#include <assert.h>
#include <heap.h>
#include <string.h>
#include <spinlock.h>
#include <irql.h>

static int num_cpus = 1;
static struct cpu cpu_table[ARCH_MAX_CPU_ALLOWED];
static platform_cpu_data_t boot_cpu_data;

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

void InitCpuTable(void) {
    /*
     * num_cpus gets initialised to 1 in the .data section. Still a good time to check that the
     * binary loaded correctly.
     */
    assert(num_cpus == 1);
    InitCpuTableEntry(0);
}

void InitBootstrapCpu(void) {
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
