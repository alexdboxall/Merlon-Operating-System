
#include <common.h>
#include <cpu.h>
#include <log.h>

/*
* x86/lowlevel/idt.c - Interrupt Descriptor Table
*
* The interrupt decriptor table (IDT) is essentially a lookup table for where
* the CPU should jump to when an interrupt is receieved.
*/

extern void x86LoadIdt(size_t addr);

/*
* Our trap handlers, defined in lowlevel/trap.s, which will be called
* when an interrupt occurs. 
*/
extern size_t isr_vectors;

/*
* Fill in an entry in the IDT. There are a number of 'types' of interrupt, determining
* whether interrupts are disabled automatically before calling the handler, whether
* it is a 32-bit or 16-bit entry, and whether user mode can invoke the interrupt manually.
*/
static void x86SetIdtEntry(int num, size_t isr_addr, uint8_t type)
{
	platform_cpu_data_t* cpu_data = GetCpu()->platform_specific;

	cpu_data->idt[num].isr_offset_low = (isr_addr & 0xFFFF);
	cpu_data->idt[num].isr_offset_high = (isr_addr >> 16) & 0xFFFF;
	cpu_data->idt[num].segment_selector = 0x08;
	cpu_data->idt[num].reserved = 0;
	cpu_data->idt[num].type = type;
}

/*
* Initialise the IDT. After this has occured, interrupts may be enabled.
*/
void x86InitIdt(void)
{
	platform_cpu_data_t* cpu_data = GetCpu()->platform_specific;
	
	/*
	 * Install the interrupt handlers. We set the system call interrupt vector
	 * to be invokable from usermode.
	 */
	for (int i = 0; i < 256; ++i) {
		LogWriteSerial("isrx%d seems to be at 0x%X\n", i, (&isr_vectors)[i]);
		x86SetIdtEntry(i, (&isr_vectors)[i], i == 96 ? 0xEE : 0x8E);
	}

	cpu_data->idtr.location = (size_t) &cpu_data->idt;
	cpu_data->idtr.size = sizeof(cpu_data->idt) - 1;
	
	x86LoadIdt((size_t) &cpu_data->idtr);
}