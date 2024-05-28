#include "krnlapi.h"
#include <sched.h>
#include <errno.h>
#include <arch.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <voidptr.h>
#include <sched.h>
#include <virtual.h>
#include <elf.h>
#include <signal.h>
#include <time.h>
#include <machine/config.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <os/everything.h>

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
            void* ret = mmap((void*) address, size + num_zero_bytes, prot | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
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
    dbgprintf("<UNKNOWN KRNLAPI.LIB SYMBOL>.\n");
    while (true) {
        ;
    }
}

extern void dyn_fixup_asm(void);

size_t resolve_address(char* name) {
    if (!xstrcmp(name, "_exit")) return (size_t) _exit;
    if (!xstrcmp(name, "close")) return (size_t) close;
    if (!xstrcmp(name, "dup")) return (size_t) dup;
    if (!xstrcmp(name, "dup2")) return (size_t) dup2;
    if (!xstrcmp(name, "dup3")) return (size_t) dup3;
    if (!xstrcmp(name, "__thread_local_errno_")) return (size_t) __thread_local_errno_;
    if (!xstrcmp(name, "execve")) return (size_t) execve;
    if (!xstrcmp(name, "lseek")) return (size_t) lseek;
    if (!xstrcmp(name, "mmap")) return (size_t) mmap;
    if (!xstrcmp(name, "munmap")) return (size_t) munmap;
    if (!xstrcmp(name, "mprotect")) return (size_t) mprotect;
    if (!xstrcmp(name, "open")) return (size_t) open;
    if (!xstrcmp(name, "read")) return (size_t) read;
    if (!xstrcmp(name, "write")) return (size_t) write;
    if (!xstrcmp(name, "unlink")) return (size_t) unlink;
    if (!xstrcmp(name, "rmdir")) return (size_t) rmdir;
    if (!xstrcmp(name, "isatty")) return (size_t) isatty;
    if (!xstrcmp(name, "waitpid")) return (size_t) waitpid;
    if (!xstrcmp(name, "fork")) return (size_t) fork;
    if (!xstrcmp(name, "ioctl")) return (size_t) ioctl;
    if (!xstrcmp(name, "stat")) return (size_t) stat;
    if (!xstrcmp(name, "fstat")) return (size_t) fstat;
    if (!xstrcmp(name, "lstat")) return (size_t) lstat;
    if (!xstrcmp(name, "chdir")) return (size_t) chdir;
    if (!xstrcmp(name, "fchdir")) return (size_t) fchdir;
    if (!xstrcmp(name, "signal")) return (size_t) signal;
    if (!xstrcmp(name, "kill")) return (size_t) kill;
    if (!xstrcmp(name, "raise")) return (size_t) raise;
    if (!xstrcmp(name, "sched_yield")) return (size_t) sched_yield;
    if (!xstrcmp(name, "OsGetFreeMemoryKilobytes")) return (size_t) OsGetFreeMemoryKilobytes;
    if (!xstrcmp(name, "OsGetTotalMemoryKilobytes")) return (size_t) OsGetTotalMemoryKilobytes;
    if (!xstrcmp(name, "OsGetVersion")) return (size_t) OsGetVersion;
    if (!xstrcmp(name, "OsSetLocalTime")) return (size_t) OsSetLocalTime;
    if (!xstrcmp(name, "OsGetLocalTime")) return (size_t) OsGetLocalTime;
    if (!xstrcmp(name, "OsSetTimezone")) return (size_t) OsSetTimezone;
    if (!xstrcmp(name, "OsGetTimezone")) return (size_t) OsGetTimezone;
    if (!xstrcmp(name, "nanosleep")) return (size_t) nanosleep;
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
    *((size_t*) relocation_entry.r_offset) = resolved_address;
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
    if (res == ENOTRECOVERABLE) {
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

    /*
     * Needed, otherwise it will use fd = 0, which is meant to be for stdin.
     */
    close(fd);

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