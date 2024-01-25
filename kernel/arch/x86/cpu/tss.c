
#include <heap.h>
#include <machine/tss.h>
#include <arch.h>
#include <machine/gdt.h>
#include <cpu.h>

extern void x86LoadTss(size_t selector);

void x86InitTss(void) {
    platform_cpu_data_t* cpu_data = GetCpu()->platform_specific;
    
    cpu_data->tss = AllocHeap(sizeof(struct tss));
    cpu_data->tss->link = 0x10;
    cpu_data->tss->esp0 = 0;
    cpu_data->tss->ss0 = 0x10;
    cpu_data->tss->iopb = sizeof(struct tss);

    uint16_t selector = x86AddTssToGdt(cpu_data->tss);
    x86LoadTss(selector);
}