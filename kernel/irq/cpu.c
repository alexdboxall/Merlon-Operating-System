#include <cpu.h>
#include <arch.h>
#include <assert.h>
#include <heap.h>
#include <string.h>
#include <irql.h>

static int num_cpus = 1;
static struct cpu cpu_table[ARCH_MAX_CPU_ALLOWED];

static void InitCpuTableEntry(int index) {
    cpu_table[index].cpu_number = index;
    cpu_table[index].platform_specific = AllocHeapZero(sizeof(platform_cpu_data_t));
    cpu_table[index].irql = IRQL_INIT;
}

void InitBootstrapCpu(void) {
    /*
     * num_cpus gets initialised to 1 in the .data section. Still a good time to check that the
     * binary loaded correctly.
     */
    assert(num_cpus == 1);
    
    InitCpuTableEntry(0);
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

struct cpu* GetCpu(void) {
    return cpu_table + ArchGetCurrentCpuIndex();
}
