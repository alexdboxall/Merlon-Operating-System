#include "krnlapi.h"
#include <sched.h>
#include <errno.h>
#include <arch.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <voidptr.h>
#include <sched.h>
#include <virtual.h>
#include <machine/elf.h>
#include <machine/config.h>
#include <sys/mman.h>

struct dyn_data {
    struct Elf32_Ehdr* elf_header;
    struct Elf32_Shdr* reldyn;
    struct Elf32_Shdr* reladyn;
    struct Elf32_Shdr* dynsym;
    struct Elf32_Shdr* dynstr;
    size_t* got;
};

static bool is_elf_valid(struct Elf32_Ehdr* header) {
    if (header->e_ident[EI_MAG0] != ELFMAG0) return false;
    if (header->e_ident[EI_MAG1] != ELFMAG1) return false;
    if (header->e_ident[EI_MAG2] != ELFMAG2) return false;
    if (header->e_ident[EI_MAG3] != ELFMAG3) return false;
    return true;
}

static char* elf_lookup_string(void* data, int offset) {
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

static int load_program_headers(void* data, int fd) {
    (void) fd;

    struct Elf32_Ehdr* elf_header = (struct Elf32_Ehdr*) data;
	struct Elf32_Phdr* prog_headers = (struct Elf32_Phdr*) AddVoidPtr(data, elf_header->e_phoff);

    for (int i = 0; i < elf_header->e_phnum; ++i) {
        struct Elf32_Phdr* prog_header = prog_headers + i;

        size_t address = prog_header->p_vaddr;
		size_t offset = prog_header->p_offset;
		size_t size = prog_header->p_filesz;
		size_t type = prog_header->p_type;
		uint32_t flags = prog_header->p_flags;
		size_t num_zero_bytes = prog_header->p_memsz - size;

		if (type == PHT_LOAD) {
			int prot = 0;
			if (flags & PF_X) prot |= PROT_EXEC;
			if (flags & PF_R) prot |= PROT_READ;
			if (flags & PF_W) prot |= PROT_WRITE;

            if (address & (ARCH_PAGE_SIZE - 1)) {
                errno = ENOEXEC;
                return -1;
            }

            /*
             * TODO: in the future, use file mapping for non-writable sections
             */
            void* ret = mmap((void*) address, size + num_zero_bytes, prot | VM_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
            if (ret == MAP_FAILED) {
                errno = ENOMEM;
                return -1;    
            }

            xmemcpy((void*) ret, (const void*) AddVoidPtr(data, offset), size);

            if ((flags & PF_W) != 0) {
                int res = mprotect((void*) address, size + num_zero_bytes, prot);
                if (res != 0) {
                    return -1;
                }
            }
		}
    }
	
	return 0;
}

static void loltest(void) {
    dbgprintf("<UNKNOWN SYMBOL>.\n");
    while (true) {
        ;
    }
}

extern void dyn_fixup_asm(void);

size_t resolve_address(char* name) {
    if (!xstrcmp(name, "open")) return (size_t) open;
    if (!xstrcmp(name, "write")) return (size_t) write;
    if (!xstrcmp(name, "close")) return (size_t) close;
    if (!xstrcmp(name, "sched_yield")) return (size_t) sched_yield;

    return (size_t) loltest;
}

size_t dyn_fixup(struct dyn_data* link_info, size_t index) {
    struct Elf32_Rel* relocation_table = (struct Elf32_Rel*) link_info->reldyn->sh_addr;
    struct Elf32_Rel relocation_entry = relocation_table[index / sizeof(struct Elf32_Rel)];
    struct Elf32_Sym* symbol_table = (struct Elf32_Sym*) link_info->dynsym->sh_addr;
    struct Elf32_Sym symbol = symbol_table[relocation_entry.r_info >> 8];
    char* string_table = (char*) link_info->dynstr->sh_addr;
    char* name = string_table + symbol.st_name;

    size_t resolved_address = resolve_address(name);
    // TODO: need to find the right address to write this back to...
    //link_info->got[index / sizeof(size_t) + 2] = resolved_address;
    return resolved_address;
}

// TODO: once we get multiple libraries, this will need to be adjusted
struct dyn_data link_info = {0};

static int link_executable_to_krnlapi(void* data) {
    struct Elf32_Ehdr* elf_header = (struct Elf32_Ehdr*) data;
	struct Elf32_Shdr* sect_headers = (struct Elf32_Shdr*) AddVoidPtr((void*) elf_header, elf_header->e_shoff);

    link_info.elf_header = elf_header;

    for (int i = 0; i < elf_header->e_shnum; ++i) {
        struct Elf32_Shdr* sect_header = sect_headers + i;
        char* name = elf_lookup_string(data, sect_header->sh_name);

        if (!xstrcmp(name, ".rel.dyn")) {
            link_info.reldyn = sect_header;
        }
        if (!xstrcmp(name, ".rela.dyn")) {
            link_info.reladyn = sect_header;
        }
        if (!xstrcmp(name, ".got.plt")) {
            size_t* got = (size_t*) sect_header->sh_addr;
            link_info.got = got;
            got[1] = (size_t) &link_info;
            got[2] = (size_t) dyn_fixup_asm;
        }
        if (!xstrcmp(name, ".dynstr")) {
            link_info.dynstr = sect_header;
        }
        if (!xstrcmp(name, ".dynsym")) {
            link_info.dynsym = sect_header;
        }
    }

    return 0;
}

int load_elf(void* data, int fd, size_t* entry_point) {
    struct Elf32_Ehdr* elf_header = (struct Elf32_Ehdr*) data;

    if (load_program_headers(data, fd) != 0) {
        return -1;
    }

    *entry_point = elf_header->e_entry;
    if (close(fd) != 0) {
        return -1;
    }

    return link_executable_to_krnlapi(elf_header);
}

void execve_core(size_t entry, size_t argc, char* const argv[], char* const envp[], size_t stack);

int execve(const char* pathname, char* const argv[], char* const envp[]) {
    int fd = open(pathname, O_RDONLY, 0);
    if (fd == -1) {
        return -1;
    }

    off_t size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    size_t entry_point;
    int res = _system_call(SYSCALL_PREPEXEC, 0, 0, 0, 0, 0);
    if (res == EUNRECOVERABLE) {
        goto unrecoverable_fail;
    } else if (res != 0) {
        errno = res;
        return -1;
    }

    struct Elf32_Ehdr* elf_header = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (elf_header == MAP_FAILED) {
        errno = ENOMEM;
        return -1;
    }

    if (!is_elf_valid(elf_header)) {
        goto unrecoverable_fail;
    }

    res = load_elf(elf_header, fd, &entry_point);
    if (res != 0) {
        goto unrecoverable_fail;
    }

    int argc = 0;
    while (argv[argc] != NULL) {
        ++argc;
    }

    execve_core(entry_point, argc, argv, envp, ARCH_USER_STACK_LIMIT);

unrecoverable_fail:
    /*
     * The program image is corrupt if we get to this state. We obviously can't
     * allow execve() to return in this state, so we'll just kill the process.
     */
     _system_call(SYSCALL_EXIT, -1, 0, 0, 0, 0);
    while (true) {
        ;
    }
}