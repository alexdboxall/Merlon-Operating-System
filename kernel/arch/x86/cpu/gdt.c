
#include <common.h>
#include <cpu.h>
#include <machine/gdt.h>

extern void x86LoadGdt(size_t addr);

static struct gdt_entry CreateGdtEntry(size_t base, size_t limit, uint8_t access, uint8_t gran)
{
	return (struct gdt_entry) {
		.base_low 			  = base & 0xFFFF, 
		.base_middle 	  	  = (base >> 16) & 0xFF, 
		.base_high 			  = (base >> 24) & 0xFF, 
		.limit_low 			  = limit & 0xFFFF,
		.flags_and_limit_high = ((limit >> 16) & 0xF) | ((gran & 0xF) << 4),
		.access 			  = access,
	};
}

void x86InitGdt(void)
{
	platform_cpu_data_t* cpu_data = GetCpu()->platform_specific;

	cpu_data->gdt[0] = CreateGdtEntry(0, 0, 0, 0);				 // null segment
	cpu_data->gdt[1] = CreateGdtEntry(0, 0xFFFFFFFF, 0x9A, 0xC); // kernel code
	cpu_data->gdt[2] = CreateGdtEntry(0, 0xFFFFFFFF, 0x92, 0xC); // kernel data
	cpu_data->gdt[3] = CreateGdtEntry(0, 0xFFFFFFFF, 0xFA, 0xC); // user code
	cpu_data->gdt[4] = CreateGdtEntry(0, 0xFFFFFFFF, 0xF2, 0xC); // user data

	cpu_data->gdtr.size = sizeof(cpu_data->gdt) - 1;
	cpu_data->gdtr.location = (size_t) &cpu_data->gdt;

	x86LoadGdt((size_t) &cpu_data->gdtr);
}

/*
 * Returns the selector used in the GDT.
 */
uint16_t x86AddTssToGdt(struct tss* tss)
{
	platform_cpu_data_t* cpu_data = GetCpu()->platform_specific;
	cpu_data->gdt[5] = CreateGdtEntry((size_t) tss, sizeof(struct tss), 0x89, 0x0);
	return 5 * 0x8;
}