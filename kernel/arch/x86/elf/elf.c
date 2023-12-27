#include <common.h>
#include <errno.h>
#include <log.h>
#include <vfs.h>
#include <virtual.h>
#include <voidptr.h>
#include <string.h>
#include <driver.h>
#include <sys/stat.h>
#include <assert.h>
#include <irql.h>
#include <machine/elf.h>
#include <panic.h>

/****
 * *** TODO:
 * *** @@@
 * ***
 * *** DRIVER PAGES THAT GET PAGED OUT NEED TO HAVE RELOCATIONS REAPPLIED TO THEM!!!!
 * *** (AS IS, WHEN IT GETS PAGED BACK IN, IT GETS THE ORIGINAL, NON-RELOCATION APPLIED PAGE BACK)
 * ***
 * ***
 * 
 * It'd also be good to allow drivers to ask for re-relocation, e.g. so that unresolved symbols can instead of 
 * panicking, just be left unresolved, and then the driver can call RequireDriver(...) and then "RelocateDriver"(...)
 * to use symbols as normal, instead of needing to use GetSymbolAddress
*/


static bool PAGEABLE_CODE_SECTION IsElfValid(struct Elf32_Ehdr* header) {
    if (header->e_ident[EI_MAG0] != ELFMAG0) return false;
    if (header->e_ident[EI_MAG1] != ELFMAG1) return false;
    if (header->e_ident[EI_MAG2] != ELFMAG2) return false;
    if (header->e_ident[EI_MAG3] != ELFMAG3) return false;

    /* TODO: check for other things, such as the platform, etc. */

    return true;
}

static size_t PAGEABLE_CODE_SECTION ElfGetSizeOfImageIncludingBss(void* data, bool relocate) {
    struct Elf32_Ehdr* elf_header = (struct Elf32_Ehdr*) data;
	struct Elf32_Phdr* prog_headers = (struct Elf32_Phdr*) AddVoidPtr(data, elf_header->e_phoff);

    size_t base_point = relocate ? 0xD0000000U : 0x10000000;
    size_t total_size = 0;

    for (int i = 0; i < elf_header->e_phnum; ++i) {
        struct Elf32_Phdr* prog_header = prog_headers + i;

        size_t address = prog_header->p_vaddr;
		size_t size = prog_header->p_filesz;
		size_t num_zero_bytes = prog_header->p_memsz - size;
		size_t type = prog_header->p_type;

		if (type == PHT_LOAD) {
			if (!relocate) {
                Panic(PANIC_NOT_IMPLEMENTED);
            
			} else {
                if (address - base_point + size + num_zero_bytes >= total_size) {
                    total_size = address - base_point + size + num_zero_bytes;
                }
			}
		}
    }

    return (total_size + ARCH_PAGE_SIZE - 1) & (~(ARCH_PAGE_SIZE - 1));
}

static int PAGEABLE_CODE_SECTION ElfLoadProgramHeaders(void* data, size_t relocation_point, bool relocate, struct open_file* file) {
    struct Elf32_Ehdr* elf_header = (struct Elf32_Ehdr*) data;
	struct Elf32_Phdr* prog_headers = (struct Elf32_Phdr*) AddVoidPtr(data, elf_header->e_phoff);

    size_t base_point = relocate ? 0xD0000000U : 0x10000000;

    for (int i = 0; i < elf_header->e_phnum; ++i) {
        struct Elf32_Phdr* prog_header = prog_headers + i;

        size_t address = prog_header->p_vaddr;
		size_t offset = prog_header->p_offset;
		size_t size = prog_header->p_filesz;
		size_t type = prog_header->p_type;
		uint32_t flags = prog_header->p_flags;
		size_t num_zero_bytes = prog_header->p_memsz - size;

		if (type == PHT_LOAD) {
			if (!relocate) {
                return ENOSYS;
            
			} else {
				size_t addr = address + relocation_point - base_point;
				size_t remainder = size & (ARCH_PAGE_SIZE - 1);

				LogWriteSerial("Loading from 0x%X -> size 0x%X, and it is: %s %s %s\n",
					addr,
					size,
					(flags & PF_R) ? "READABLE" : "-",
					(flags & PF_W) ? "WRITABLE" : "-",
					(flags & PF_X) ? "EXECUTABLE" : "-"
				);
				
				int page_flags = 0;
				if (flags & PF_X) page_flags |= VM_EXEC;
				if (flags & PF_W) page_flags |= VM_WRITE;
				if (flags & PF_R) page_flags |= VM_READ;

				/*
				 * We don't actually want to write to the executable file, so we must just copy to the page as normal
				 * instead of using a file-backed page.
				 */
				if (flags & PF_W) {
					size_t pages = (size + num_zero_bytes + (ARCH_PAGE_SIZE - 1)) / ARCH_PAGE_SIZE;

					for (size_t i = 0; i < pages; ++i) {
						SetVirtPermissions(addr + i * ARCH_PAGE_SIZE, page_flags, (VM_READ | VM_WRITE | VM_EXEC) & ~page_flags);
					}

					memcpy((void*) addr, (const void*) AddVoidPtr(data, offset), size);

				} else {
					size_t pages = (size - remainder) / ARCH_PAGE_SIZE;

					if (addr & (ARCH_PAGE_SIZE - 1)) {
						return EINVAL;
					}
					if (pages > 0) {
						UnmapVirt(addr, pages * ARCH_PAGE_SIZE);
						size_t v = MapVirt(relocation_point, addr, pages * ARCH_PAGE_SIZE, VM_RELOCATABLE | VM_FILE | page_flags, file, offset);
						if (v != addr) {
							return ENOMEM;
						}
					}

					if (remainder > 0) {
						SetVirtPermissions(addr + pages * ARCH_PAGE_SIZE, page_flags | VM_WRITE, (VM_READ | VM_EXEC) & ~page_flags);
						memcpy((void*) AddVoidPtr(addr, pages * ARCH_PAGE_SIZE), (const void*) AddVoidPtr(data, offset + pages * ARCH_PAGE_SIZE), remainder);
						SetVirtPermissions(addr + pages * ARCH_PAGE_SIZE, 0, VM_WRITE);
					}
				}
			}
		}
    }
	
	return 0;
}

static char* PAGEABLE_CODE_SECTION ElfLookupString(void* data, int offset) {
    struct Elf32_Ehdr* elf_header = (struct Elf32_Ehdr*) data;

	if (elf_header->e_shstrndx == SHN_UNDEF) {
		return NULL;
	}

	struct Elf32_Shdr* sect_headers = (struct Elf32_Shdr*) AddVoidPtr(data, elf_header->e_shoff);

	char* string_table = (char*) AddVoidPtr(data, sect_headers[elf_header->e_shstrndx].sh_offset);
	if (string_table == NULL) {
		return NULL;
	}

	return string_table + offset;
}

static size_t PAGEABLE_CODE_SECTION ElfGetSymbolValue(void* data, int table, size_t index, bool* error, size_t relocation_point, size_t base_address) {
    *error = false;

	if (table == SHN_UNDEF || index == SHN_UNDEF) {
		*error = true;
		return 0;
	}

    struct Elf32_Ehdr* elf_header = (struct Elf32_Ehdr*) data;
    struct Elf32_Shdr* sect_headers = (struct Elf32_Shdr*) AddVoidPtr(data, elf_header->e_shoff);
	struct Elf32_Shdr* symbol_table = sect_headers + table;
	struct Elf32_Shdr* string_table = sect_headers + symbol_table->sh_link;

	size_t num_symbol_table_entries = symbol_table->sh_size / symbol_table->sh_entsize;
	if (index >= num_symbol_table_entries) {
		*error = true;
		return 0;
	}

	struct Elf32_Sym* symbol = ((struct Elf32_Sym*) AddVoidPtr(data, symbol_table->sh_offset)) + index;

	if (symbol->st_shndx == SHN_UNDEF) {
		const char* name = (const char*) AddVoidPtr(data, string_table->sh_offset + symbol->st_name);

		LogWriteSerial("Looking for symbol with name: %s...)\n", name);
		size_t target = GetSymbolAddress(name);
		LogWriteSerial("got 0x%X\n", target);

		if (target == 0) {
			if (!(ELF32_ST_BIND(symbol->st_info) & STB_WEAK)) {
				*error = true;
			}
			return 0;

		} else {
			return target;
		}

	} else if (symbol->st_shndx == SHN_ABS) {
		return symbol->st_value;

	} else {
		return symbol->st_value + (relocation_point - base_address);
	}
}

static bool PAGEABLE_CODE_SECTION ElfPerformRelocation(void* data, size_t relocation_point, struct Elf32_Shdr* section, struct Elf32_Rel* relocation_table)
{
	LogWriteSerial("about to perform a relocation!\n");

	size_t base_address = 0xD0000000U;

	size_t addr = (size_t) relocation_point - base_address + relocation_table->r_offset;

	size_t* ref = (size_t*) addr;

	int symbolValue = 0;
	if (ELF32_R_SYM(relocation_table->r_info) != SHN_UNDEF) {
		bool error = false;
		symbolValue = ElfGetSymbolValue(data, section->sh_link, ELF32_R_SYM(relocation_table->r_info), &error, relocation_point, base_address);
		if (error) {
			return false;
		}
	}
	
	if ((GetVirtPermissions(addr) & VM_WRITE) == 0) {
		SetTemporaryWriteEnable(addr, true);
	}
	
	int type = ELF32_R_TYPE(relocation_table->r_info);
	
	if (type == R_386_32) {
		*ref = DO_386_32(symbolValue, *ref);

	} else if (type == R_386_PC32) {
		*ref = DO_386_PC32(symbolValue, *ref, (size_t) ref);

	} else if (type == R_386_RELATIVE) {
		*ref = DO_386_RELATIVE((relocation_point - base_address), *ref);

	} else {
		LogWriteSerial("some whacko type...\n");
		SetTemporaryWriteEnable(addr, false);
		return false;
	}
	
	SetTemporaryWriteEnable(addr, false);
	return true;
}

static bool PAGEABLE_CODE_SECTION ElfPerformRelocations(void* data, size_t relocation_point) {
    struct Elf32_Ehdr* elf_header = (struct Elf32_Ehdr*) data;
	struct Elf32_Shdr* sect_headers = (struct Elf32_Shdr*) AddVoidPtr(data, elf_header->e_shoff);

	for (int i = 0; i < elf_header->e_shnum; ++i) {
		LogWriteSerial("starting relocation %d / %d\n", i, elf_header->e_shnum);
		struct Elf32_Shdr* section = sect_headers + i;

		if (section->sh_type == SHT_REL) {
			struct Elf32_Rel* relocation_tables = (struct Elf32_Rel*) AddVoidPtr(data, section->sh_offset);
			int count = section->sh_size / section->sh_entsize;

			if (strcmp(ElfLookupString(data, section->sh_name), ".rel.dyn")) {
				continue;
			}

			for (int index = 0; index < count; ++index) {
				bool success = ElfPerformRelocation(data, relocation_point, section, relocation_tables + index);
				if (!success) {
					LogWriteSerial("failed to do a relocation!! (%d)\n", index);
					return false;
				}
				LogWriteSerial("successful relocation!! (%d)\n", index);
			}


		} else if (section->sh_type == SHT_RELA) {
			LogDeveloperWarning("[ElfPerformRelocations]: unsupported section type: SHT_RELA\n");
            return false;
		}
	}

	return true;
}

static PAGEABLE_CODE_SECTION int ElfLoad(void* data, size_t* relocation_point, bool relocate, size_t* entry_point, struct open_file* file) {
    EXACT_IRQL(IRQL_STANDARD);

    struct Elf32_Ehdr* elf_header = (struct Elf32_Ehdr*) data;

    if (!IsElfValid(elf_header)) {
        return EINVAL;
    }

    /*
    * To load a driver, we need the section headers.
    */
    if (elf_header->e_shnum == 0 && relocate) {
        return EINVAL;
    }

    /*
    * We always need the program headers.
    */
    if (elf_header->e_phnum == 0) {
        return EINVAL;
    }

    /*
    * Load into memory.
    */
    size_t size = ElfGetSizeOfImageIncludingBss(data, relocate);

    if (relocate) {
		*relocation_point = MapVirt(0, 0, size, VM_READ, NULL, 0);
		LogWriteSerial("RELOCATION POINT AT 0x%X\n", *relocation_point);
    	ElfLoadProgramHeaders(data, *relocation_point, relocate, file);
        bool success = ElfPerformRelocations(data, *relocation_point);
        if (success) {
            return 0;

        } else {
            return EINVAL;
        }

    } else {
		(void) entry_point;
		return ENOSYS;
    }
}

int PAGEABLE_CODE_SECTION ArchLoadDriver(size_t* relocation_point, struct open_file* file) {
    EXACT_IRQL(IRQL_STANDARD);

    off_t file_size;
	int res = GetFileSize(file, &file_size);
	if (res != 0) {
		return res;
	}

    size_t file_rgn = MapVirt(0, 0, file_size, VM_READ | VM_FILE, file, 0);
	LogWriteSerial("mapped a region to the file: 0x%X, of size 0x%X\n", file_rgn, (int) file_size);
    res = ElfLoad((void*) file_rgn, relocation_point, true, NULL, file);

	struct Elf32_Ehdr* elf_header = (struct Elf32_Ehdr*) file_rgn;
    struct Elf32_Shdr* sect_headers = (struct Elf32_Shdr*) (file_rgn + elf_header->e_shoff);

	for (int i = 0; i < elf_header->e_shnum; ++i) {
		const char* sh_name = ElfLookupString((void*) file_rgn, sect_headers[i].sh_name);
		if (!strcmp(sh_name, ".lockedtext") || !strcmp(sh_name, ".lockeddata")) {
			// it's okay to lock extra memory - it wouldn't be ok if we did it the other way around though
			// (assumed everything was locked, and only unlocked parts of it)
			size_t start_addr = (sect_headers[i].sh_addr - 0xD0000000U + *relocation_point) & ~(ARCH_PAGE_SIZE - 1);
			size_t num_pages = (sect_headers[i].sh_size + ARCH_PAGE_SIZE - 1) / ARCH_PAGE_SIZE;
			while (num_pages--) {
				LogWriteSerial("Locking driver memory here: 0x%X\n", start_addr);
				LockVirt(start_addr);
				start_addr += ARCH_PAGE_SIZE;
			}
		}
	}

    UnmapVirt(file_rgn, file_size);

    return res;
}

void PAGEABLE_CODE_SECTION ArchLoadSymbols(struct open_file* file, size_t adjust) {
	off_t size;
	int res = GetFileSize(file, &size);
	if (res != 0) {
		Panic(PANIC_BAD_KERNEL);
	}

	size_t mem = MapVirt(0, 0, size, VM_READ | VM_FILE, file, 0);

    struct Elf32_Ehdr* elf_header = (struct Elf32_Ehdr*) mem;

    /*
    * These should never happen - otherwise our kernel shouldn't be running!
    */
    if (!IsElfValid(elf_header) || elf_header->e_shoff == 0) {
		Panic(PANIC_BAD_KERNEL);
    }

    struct Elf32_Shdr* section_headers = (struct Elf32_Shdr*) (size_t) (mem + elf_header->e_shoff);
    size_t symbol_table_offset = 0;
    size_t symbol_table_length = 0;
    size_t string_table_offset = 0;
    size_t string_table_length = 0;

    /*
    * Find the address and size of the symbol and string tables.
    */
    for (int i = 0; i < elf_header->e_shnum; ++i) {
        size_t file_offset = (section_headers + i)->sh_offset;
        size_t address = (section_headers + elf_header->e_shstrndx)->sh_offset + (section_headers + i)->sh_name;

        char* name_buffer = (char*) (mem + address);

        if (!strcmp(name_buffer, ".symtab")) {
            symbol_table_offset = file_offset;
            symbol_table_length = (section_headers + i)->sh_size;

        } else if (!strcmp(name_buffer, ".strtab")) {
            string_table_offset = file_offset;
            string_table_length = (section_headers + i)->sh_size;
        }
    }
    
    if (symbol_table_offset == 0 || string_table_offset == 0 || symbol_table_length == 0 || string_table_length == 0) {
        Panic(PANIC_BAD_KERNEL);
    }

    struct Elf32_Sym* symbol_table = (struct Elf32_Sym*) (mem + symbol_table_offset);
    const char* string_table = (const char*) (mem + string_table_offset);

    /*
    * Register all of the visible symbols we find.
    */
    for (size_t i = 0; i < symbol_table_length / sizeof(struct Elf32_Sym); ++i) {
        struct Elf32_Sym symbol = symbol_table[i];

        if (symbol.st_value == 0) { 
            continue;
        }

		/*
		 * Skip "hidden" and "internal" symbols
		 */
		if ((symbol.st_other & 3) != 0) {
			continue;
		}

		/*
		 * No need for strdup, as it gets converted to the weird radix trie format.
		 */
        AddSymbol(string_table + symbol.st_name, symbol.st_value + adjust);
    }

	UnmapVirt(mem, size);
}