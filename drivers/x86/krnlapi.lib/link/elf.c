
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <voidptr.h>
#include <machine/elf.h>
#include <machine/config.h>
#include <sys/mman.h>

static bool is_elf_valid(struct Elf32_Ehdr* header) {
    if (header->e_ident[EI_MAG0] != ELFMAG0) return false;
    if (header->e_ident[EI_MAG1] != ELFMAG1) return false;
    if (header->e_ident[EI_MAG2] != ELFMAG2) return false;
    if (header->e_ident[EI_MAG3] != ELFMAG3) return false;
    return true;
}


static int load_program_headers(void* data, int fd) {
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
			if (flags & PF_W) prot |= PROT_WRITE;
			if (flags & PF_R) prot |= PROT_READ;

            if (address & (ARCH_PAGE_SIZE - 1)) {
                return EINVAL;
            }

            /*
             * TODO: in the future, use file mapping for non-writable sections
             */
            void* ret = mmap((void*) (address & (ARCH_PAGE_SIZE - 1)), size + num_zero_bytes, prot, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
            if (ret == MAP_FAILED) {
                return ENOMEM;
            }

            memcpy((void*) ret, (const void*) AddVoidPtr(data, offset), size);
		}
    }
	
	return 0;
}

int load_elf(const char* filename, size_t* entry_point) {
    int fd = open(filename, O_RDONLY, 0);
    if (fd == -1) {
        return errno;
    }

    off_t size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    struct Elf32_Ehdr* elf_header = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);

    if (!is_elf_valid(elf_header)) {
        return EINVAL;
    }

	load_program_headers(elf_header, fd);
    *entry_point = elf_header->e_entry;

    close(fd);
}