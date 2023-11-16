#pragma once

#include <common.h>
#include <arch.h>

struct vas;
struct thread;

struct cpu {
    struct vas* current_vas;
    struct thread* current_thread;
    platform_cpu_data_t* platform_specific;
    size_t cpu_number;
    int irql;
};

void InitBootstrapCpu(void);
void InitOtherCpu(void);

struct cpu* GetCpu(void);
int GetCpuCount(void);
struct cpu* GetCpuAtIndex(int index);