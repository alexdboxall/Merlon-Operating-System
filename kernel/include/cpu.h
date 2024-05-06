#pragma once

#include <common.h>
#include <arch.h>
#include <spinlock.h>

struct vas;
struct thread;
struct tree;

struct cpu {
    struct vas* current_vas;
    struct thread* current_thread;
    platform_cpu_data_t* platform_specific;
    size_t cpu_number;
    int irql;

    struct heap_adt* deferred_functions;
    struct heap_adt* irq_deferred_functions;
    bool init_irql_done;
    bool postponed_task_switch;

    struct tree* global_vas_mappings;
    struct spinlock global_mappings_lock;
};

void InitCpuTable(void);
void InitBootstrapCpu(void);
void InitOtherCpu(void);

extern struct cpu cpu_table[ARCH_MAX_CPU_ALLOWED];

#define GetCpu(...) (cpu_table + ArchGetCurrentCpuIndex())

int GetCpuCount(void);
struct cpu* GetCpuAtIndex(int index);