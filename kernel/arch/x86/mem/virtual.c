#include <machine/virtual.h>
#include <assert.h>
#include <string.h>
#include <arch.h>
#include <physical.h>
#include <arch.h>
#include <log.h>
#include <avl.h>
#include <virtual.h>

__attribute__((fastcall)) size_t x86KernelMemoryToPhysical(size_t virtual)
{
	assert(virtual < 0x400000);
	return 0xC0000000 + virtual;
}

static struct vas vas_table[ARCH_MAX_CPU_ALLOWED];
static platform_vas_data_t vas_data_table[ARCH_MAX_CPU_ALLOWED];

static size_t kernel_page_directory[1024] __attribute__((aligned(ARCH_PAGE_SIZE)));
static size_t first_page_table[1024] __attribute__((aligned(ARCH_PAGE_SIZE)));

#define x86_PAGE_PRESENT		1
#define x86_PAGE_WRITE			2
#define x86_PAGE_USER			4

static void x86AllocatePageTable(struct vas* vas, size_t table_num) {
	size_t* page_dir = vas->arch_data->v_page_directory;
	size_t page_dir_phys = AllocPhys();
	page_dir[table_num] = page_dir_phys | x86_PAGE_PRESENT | x86_PAGE_WRITE;
	ArchFlushTlb(vas);
	memset((void*) (0xFFC00000 + table_num * ARCH_PAGE_SIZE), 0, ARCH_PAGE_SIZE);
}

static void x86MapPage(struct vas* vas, size_t physical, size_t virtual, int flags) {
	size_t table_num = virtual / 0x400000;
	size_t page_num = (virtual % 0x400000) / ARCH_PAGE_SIZE;
	size_t* page_dir = vas->arch_data->v_page_directory;

	if (!(page_dir[table_num] & x86_PAGE_PRESENT)) {
		x86AllocatePageTable(vas, table_num);
	}

	((size_t*) (0xFFC00000 + table_num * ARCH_PAGE_SIZE))[page_num] = physical | flags;
	ArchFlushTlb(vas);
}

size_t ArchVirtualToPhysical(size_t virtual) {
	struct vas* vas = GetVas();

	size_t table_num = virtual / 0x400000;
	size_t page_num = (virtual % 0x400000) / ARCH_PAGE_SIZE;
	size_t* page_dir = vas->arch_data->v_page_directory;

	if (!(page_dir[table_num] & x86_PAGE_PRESENT)) {
		return 0;
	}

	size_t entry = ((size_t*) (0xFFC00000 + table_num * ARCH_PAGE_SIZE))[page_num];
	if (entry & x86_PAGE_PRESENT) {
		return entry & (~0xFFF);
	} else {
		return 0;
	}
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
	x86SetCr3(vas->arch_data->p_page_directory);
}

void ArchFlushTlb(struct vas* vas) {
	ArchSetVas(vas);
}

void ArchInitVas(struct vas* vas) {
	platform_vas_data_t* data = vas->arch_data;

	(void) data;
}

void ArchInitVirt(void) {
	struct vas* vas = &vas_table[0];
	vas->arch_data = &vas_data_table[0];
	CreateVasEx(vas, VAS_NO_ARCH_INIT);

	memset(kernel_page_directory, 0, ARCH_PAGE_SIZE);
	memset(first_page_table, 0, ARCH_PAGE_SIZE);

	extern size_t _kernel_end;
	size_t max_kernel_addr = (((size_t) &_kernel_end) + 0xFFF) & ~0xFFF;

	/*
	* Map the kernel by mapping the first 1MB + kernel size up to 0xC0000000 (assumes the kernel is 
    * less than 4MB). This needs to match what kernel_entry.s exactly.
	*/

	kernel_page_directory[768] = ((size_t) first_page_table - 0xC0000000) | x86_PAGE_PRESENT | x86_PAGE_WRITE | x86_PAGE_USER;

	/* <= is required to make it match kernel_entry.s */
	size_t num_pages = (max_kernel_addr - 0xC0000000) / ARCH_PAGE_SIZE;
    for (size_t i = 0; i < num_pages; ++i) {
		first_page_table[i] = (i * ARCH_PAGE_SIZE) | x86_PAGE_PRESENT;
	}

	/*
	* Set up recursive mapping by mapping the 1024th page table to
	* the page directory. See arch_vas_set_entry for an explaination of why we do this.
    * "Locking" this page directory entry is the only we can lock the final page of virtual
    * memory, due to the recursive nature of this entry.
	*/
	kernel_page_directory[1023] = ((size_t) kernel_page_directory - 0xC0000000) | x86_PAGE_PRESENT | x86_PAGE_WRITE;

	vas->arch_data->p_page_directory = ((size_t) kernel_page_directory) - 0xC0000000;
	vas->arch_data->v_page_directory = kernel_page_directory;
	
	SetVas(vas);

	/* 
	* The virtual memory manager is now initialised, so we can fill in 
	* the rest of the kernel state. This is important as we need all of 
	* the kernel address spaces to share page tables, so we must allocate
	* them all now so new address spaces can copy from us.
	*/
	for (int i = 769; i < 1023; ++i) {
		x86AllocatePageTable(vas, i);
	}

	/*
	 * The boot assembly code set up two page tables for us, that we no longer need.
	 * We can release that physical memory.
	 */
	extern size_t boot_page_directory;
	extern size_t boot_page_table1;
	DeallocPhys(ArchVirtualToPhysical((size_t) &boot_page_directory));
	DeallocPhys(ArchVirtualToPhysical((size_t) &boot_page_table1));
}