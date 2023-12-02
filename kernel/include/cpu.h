#pragma once

#include <common.h>
#include <arch.h>
#include <avl.h>

struct vas;
struct thread;
struct avl_tree;

struct cpu {
    struct vas* current_vas;
    struct thread* current_thread;
    platform_cpu_data_t* platform_specific;
    size_t cpu_number;
    int irql;

    struct priority_queue* deferred_functions;
    bool init_irql_done;
    bool postponed_task_switch;

    struct avl_tree* global_vas_mappings;
};

void InitCpuTable(void);
void InitBootstrapCpu(void);
void InitOtherCpu(void);

struct cpu* GetCpu(void);
int GetCpuCount(void);
struct cpu* GetCpuAtIndex(int index);