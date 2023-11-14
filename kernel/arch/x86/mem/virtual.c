#include <machine/virtual.h>
#include <assert.h>
#include <string.h>
#include <arch.h>
#include <physical.h>
#include <virtual.h>

__attribute__((fastcall)) size_t x86KernelMemoryToPhysical(size_t virtual)
{
	assert(virtual < 0x400000);
	return 0xC0000000 + virtual;
}

#define x86_PAGE_PRESENT		1
#define x86_PAGE_WRITE			2
#define x86_PAGE_USER			4

struct x86_page_data {
	size_t p_page_directory;		// cr3
	size_t* v_page_directory;		// what we use to access the tables
};
 
void ArchAddGlobalsToVas(struct vas* vas) {
	(void) vas;
}

void x86MapPage(struct vas* vas, size_t physical, size_t virtual, int flags) {
	/*
	 * "You are not expected to understand this."
	 */

	size_t table_num = virtual / 0x400000;
	size_t page_num = (virtual % 0x400000) / ARCH_PAGE_SIZE;
	size_t* page_dir = (size_t*) (((struct x86_page_data*) vas->arch_data)->v_page_directory);

	if (!(page_dir[table_num] & x86_PAGE_PRESENT)) {
		size_t page_dir_phys = AllocPhys();
		page_dir[table_num] = page_dir_phys | x86_PAGE_PRESENT | x86_PAGE_WRITE;
		ArchFlushTlb(vas);
		memset((void*) (0xFFC00000 + table_num * ARCH_PAGE_SIZE), 0, ARCH_PAGE_SIZE);
	}

	((size_t*) (0xFFC00000 + table_num * ARCH_PAGE_SIZE))[page_num] = physical | flags;
	ArchFlushTlb(vas);
}

void ArchUpdateMapping(struct vas* vas, struct vas_entry* entry) {
	int flags = 0;
	if (!entry->cow && entry->write) flags |= x86_PAGE_WRITE;
	if (entry->in_ram) flags |= x86_PAGE_PRESENT;

	x86MapPage(vas, entry->physical, entry->virtual, flags);
}

void ArchAddMapping(struct vas* vas, struct vas_entry* entry) {
	ArchUpdateMapping(vas, entry);
}

void ArchUnmap(struct vas* vas, struct vas_entry* entry) {
	x86MapPage(vas, 0, entry->virtual, 0);
}

void ArchSetVas(struct vas* vas) {
	extern size_t x86SetCr3(size_t);
	struct x86_page_data* cr3 = (struct x86_page_data*) vas->arch_data;
	x86SetCr3(cr3->p_page_directory);
}

void ArchFlushTlb(struct vas* vas) {
	ArchSetVas(vas);
}

int ArchGetVirtFaultType(void* fault_info) {
	(void) fault_info;

	/*
	 * TODO: cast to x86_regs* and check err_code 
	 *		 should return bitfield with either VM_READ, VM_WRITE, VM_USER, VM_EXEC
	 */
	return 0;	
}

void ArchInitVirt(void) {
	// TODO: !
}