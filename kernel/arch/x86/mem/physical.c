
#include <common.h>
#include <arch.h>
#include <assert.h>
#include <panic.h>
#include <bootloader.h>
#include <log.h>
#include <machine/virtual.h>

struct boot_memory_entry* ArchGetMemory(struct kernel_boot_info* boot_info)
{
	extern size_t _kernel_end;
	size_t max_kernel_addr = (((size_t) &_kernel_end) - 0xC0000000 + 0xFFF) & ~0xFFF;

	static int entries_used = 0;

	struct boot_memory_entry* ram_table = (struct boot_memory_entry*) (((size_t) boot_info->ram_table) + 0xC0000000);
	int table_length = boot_info->num_ram_table_entries;

	LogWriteSerial("RAM TABLE AT 0x%X. entries = %d\n", ram_table, boot_info->num_ram_table_entries);

retry:
	if (entries_used >= table_length) {
		return NULL;
	}
	
	struct boot_memory_entry* entry = ram_table + (entries_used++);
	if (BOOTRAM_GET_TYPE(entry->info) != BOOTRAM_TYPE_AVAILABLE) {
		goto retry;
	}

	if (entry->address < 0x80000) {
		if (entry->address == 0x0) {
			entry->address += 4096;
			entry->length -= 4096;
		}

		/*
		 * Try to salvage some low memory too.
		 */
		if (entry->length + entry->address >= 0x80000) {
			entry->length = 0x80000 - entry->address;
		}
		if (entry->length + entry->address >= max_kernel_addr) {
			LogDeveloperWarning("LOST SOME MEMORY WITH RANGE 0x%X -> 0x%X\n", entry->address, entry->address + entry->length);
		}
		
	} else if (entry->address < 0x100000) {
		goto retry;
	
	} else if (entry->address < max_kernel_addr) {
		/*
		* If it starts below the kernel, but ends above it, cut it off so only the
		* part above the kernel is used.
		*/
		entry->length = entry->address + entry->length - max_kernel_addr;
		entry->address = max_kernel_addr;
	}

	if (entry->length <= 0) {
		goto retry;
	}
	
	return entry;
}
