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

static bool IsElfValid(struct Elf32_Ehdr* header) {
    if (header->e_ident[EI_MAG0] != ELFMAG0) return false;
    if (header->e_ident[EI_MAG1] != ELFMAG1) return false;
    if (header->e_ident[EI_MAG2] != ELFMAG2) return false;
    if (header->e_ident[EI_MAG3] != ELFMAG3) return false;

    /* TODO: check for other things, such as the platform, etc. */

    return true;
}

static size_t ElfGetSizeOfImageIncludingBss(void* data, bool relocate) {
    struct Elf32_Ehdr* elf_header = (struct Elf32_Ehdr*) data;
	struct Elf32_Phdr* prog_headers = (struct Elf32_Phdr*) AddVoidPtr(data, elf_header->e_phoff);

    size_t base_point = relocate ? 0xD0000000U : 0x10000000;
    size_t total_size = base_point;

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

    return (total_size + ARCH_PAGE_SIZE - 1) & (~ARCH_PAGE_SIZE);
}

static size_t ElfLoadProgramHeaders(void* data, size_t relocation_point, bool relocate) {
    struct Elf32_Ehdr* elf_header = (struct Elf32_Ehdr*) data;
	struct Elf32_Phdr* prog_headers = (struct Elf32_Phdr*) AddVoidPtr(data, elf_header->e_phoff);

    size_t base_point = relocate ? 0xD0000000U : 0x10000000;
    size_t sbrk_address = base_point;

    for (int i = 0; i < elf_header->e_phnum; ++i) {
        struct Elf32_Phdr* prog_header = prog_headers + i;

        size_t address = prog_header->p_vaddr;
		size_t offset = prog_header->p_offset;
		size_t size = prog_header->p_filesz;
		size_t num_zero_bytes = prog_header->p_memsz - size;
		size_t type = prog_header->p_type;

		if (type == PHT_LOAD) {
			if (!relocate) {
                Panic(PANIC_NOT_IMPLEMENTED);
            
			} else {
				memcpy((void*) (address + relocation_point - base_point), (const void*) AddVoidPtr(data, offset), size);
				memset((void*) (address + relocation_point - base_point + size), 0, num_zero_bytes);
			}
		}
    }

    return sbrk_address;
}

static char* ElfLookupString(void* data, int offset) {
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

static size_t ElfGetSymbolValue(void* data, int table, size_t index, bool* error, size_t relocation_point, size_t base_address) {
    *error = false;

	if (table == SHN_UNDEF || index == SHN_UNDEF) {
		*error = true;
		return 0;
	}

    struct Elf32_Ehdr* elf_header = (struct Elf32_Ehdr*) data;
    struct Elf32_Shdr* sect_headers = (struct Elf32_Shdr*) AddVoidPtr(data, elf_header->e_shoff);
	struct Elf32_Shdr* symbol_table = sect_headers + table;

	size_t num_symbol_table_entries = symbol_table->sh_size / symbol_table->sh_entsize;
	if (index >= num_symbol_table_entries) {
		*error = true;
		return 0;
	}

	struct Elf32_Sym* symbol = ((struct Elf32_Sym*) AddVoidPtr(data, symbol_table->sh_offset)) + index;

	if (symbol->st_shndx == SHN_UNDEF) {
		struct Elf32_Shdr* string_table = sect_headers + symbol_table->sh_link;
		const char* name = (const char*) AddVoidPtr(data, string_table->sh_offset + symbol->st_name);
		size_t target = GetSymbolAddress(name);

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

static bool ElfPerformRelocation(void* data, size_t relocation_point, struct Elf32_Shdr* section, struct Elf32_Rel* relocation_table)
{
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
	
	int type = ELF32_R_TYPE(relocation_table->r_info);
	if (type == R_386_32) {
		*ref = DO_386_32(symbolValue, *ref);

	} else if (type == R_386_PC32) {
		*ref = DO_386_PC32(symbolValue, *ref, (size_t) ref);

	} else if (type == R_386_RELATIVE) {
		*ref = DO_386_RELATIVE((relocation_point - base_address), *ref);

	} else {
		return false;
	}

	return true;
}

static bool ElfPerformRelocations(void* data, size_t relocation_point) {
    struct Elf32_Ehdr* elf_header = (struct Elf32_Ehdr*) data;
	struct Elf32_Shdr* sect_headers = (struct Elf32_Shdr*) AddVoidPtr(data, elf_header->e_shoff);

	for (int i = 0; i < elf_header->e_shnum; ++i) {
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
					return false;
				}
			}


		} else if (section->sh_type == SHT_RELA) {
			LogDeveloperWarning("[ElfPerformRelocations]: unsupported section type: SHT_RELA\n");
            return false;
		}
	}

	return true;
}

static int ElfLoad(void* data, size_t* relocation_point, bool relocate, size_t* entry_point) {
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
		*relocation_point = MapVirt(0, 0, size, VM_READ | VM_EXEC | VM_LOCK, NULL, 0);
    	ElfLoadProgramHeaders(data, *relocation_point, relocate);

        bool success = ElfPerformRelocations(data, *relocation_point);
        if (success) {
			*entry_point = elf_header->e_entry - 0xD0000000U + ((size_t) relocation_point);
            return 0;

        } else {
            return EINVAL;
        }

    } else {
		return ENOSYS;
    }
}

int ArchLoadDriver(size_t* relocation_point, struct open_file* file) {
    EXACT_IRQL(IRQL_STANDARD);

    struct stat st;
    size_t file_size = VnodeOpStat(file->node, &st);

    size_t file_rgn = MapVirt(0, 0, file_size, VM_READ | VM_FILE, file, 0);

    int res = ElfLoad((void*) file_rgn, relocation_point, true, NULL);
    UnmapVirt(file_rgn, file_size);

    return res;
}