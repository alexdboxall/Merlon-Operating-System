#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define ELF_NIDENT	16

typedef uint16_t Elf32_Half;	// Unsigned half int
typedef uint32_t Elf32_Off;		// Unsigned offset
typedef uint32_t Elf32_Addr;	// Unsigned address
typedef uint32_t Elf32_Word;	// Unsigned int
typedef int32_t  Elf32_Sword;	// Signed int

struct Elf32_Ehdr
{
	uint8_t		e_ident[ELF_NIDENT];
	Elf32_Half	e_type;
	Elf32_Half	e_machine;
	Elf32_Word	e_version;
	Elf32_Addr	e_entry;
	Elf32_Off	e_phoff;
	Elf32_Off	e_shoff;
	Elf32_Word	e_flags;
	Elf32_Half	e_ehsize;
	Elf32_Half	e_phentsize;
	Elf32_Half	e_phnum;
	Elf32_Half	e_shentsize;
	Elf32_Half	e_shnum;
	Elf32_Half	e_shstrndx;

};

enum Elf_Ident
{
	EI_MAG0 = 0,        // 0x7F
	EI_MAG1 = 1,        // 'E'
	EI_MAG2 = 2,        // 'L'
	EI_MAG3 = 3,        // 'F'
	EI_CLASS = 4,       // Architecture (32/64)
	EI_DATA = 5,        // Byte Order
	EI_VERSION = 6,     // ELF Version
	EI_OSABI = 7,       // OS Specific
	EI_ABIVERSION = 8,  // OS Specific
	EI_PAD = 9          // Padding
};

#define ELFMAG0	0x7F    // e_ident[EI_MAG0]
#define ELFMAG1	'E'     // e_ident[EI_MAG1]
#define ELFMAG2	'L'     // e_ident[EI_MAG2]
#define ELFMAG3	'F'     // e_ident[EI_MAG3]

#define ELFDATA2LSB	(1)  // Little Endian
#define ELFCLASS32	(1)  // 32-bit Architecture

enum Elf_Type
{
	ET_NONE = 0,	    // Unkown Type
	ET_REL = 1,		    // Relocatable File
	ET_EXEC = 2		    // Executable File
};

#define EM_386		(3)  // x86 Machine Type
#define EV_CURRENT	(1)  // ELF Current Version


struct Elf32_Shdr
{
	Elf32_Word	sh_name;
	Elf32_Word	sh_type;
	Elf32_Word	sh_flags;
	Elf32_Addr	sh_addr;
	Elf32_Off	sh_offset;
	Elf32_Word	sh_size;
	Elf32_Word	sh_link;
	Elf32_Word	sh_info;
	Elf32_Word	sh_addralign;
	Elf32_Word	sh_entsize;
};

#define SHN_UNDEF	0x0000	// Undefined/Not present
#define SHN_ABS		0xFFF1	// Absolute value

enum ShT_Types
{
	SHT_NULL = 0,		// Null section
	SHT_PROGBITS = 1,   // Program information
	SHT_SYMTAB = 2,		// Symbol table
	SHT_STRTAB = 3,		// String table
	SHT_RELA = 4,		// Relocation (w/ addend)
	SHT_NOBITS = 8,		// Not present in file
	SHT_REL = 9,		// Relocation (no addend)
};

enum ShT_Attributes
{
	SHF_WRITE = 0x01, // Writable section
	SHF_ALLOC = 0x02  // Exists in memory
};

struct Elf32_Sym
{
	Elf32_Word		st_name;
	Elf32_Addr		st_value;
	Elf32_Word		st_size;
	uint8_t			st_info;
	uint8_t			st_other;
	Elf32_Half		st_shndx;
};

#define ELF32_ST_BIND(INFO)	((INFO) >> 4)
#define ELF32_ST_TYPE(INFO)	((INFO) & 0x0F)

enum StT_Bindings
{
	STB_LOCAL = 0,	// Local scope
	STB_GLOBAL = 1, // Global scope
	STB_WEAK = 2	// Weak, (ie. __attribute__((weak)))
};

enum StT_Types
{
	STT_NOTYPE = 0, // No type
	STT_OBJECT = 1, // Variables, arrays, etc.
	STT_FUNC = 2	// Methods or functions
};

struct Elf32_Rel
{
	Elf32_Addr		r_offset;
	Elf32_Word		r_info;
};

struct Elf32_Rela {
	Elf32_Addr		r_offset;
	Elf32_Word		r_info;
	Elf32_Sword		r_addend;
};

#define ELF32_R_SYM(INFO)	((INFO) >> 8)
#define ELF32_R_TYPE(INFO)	((uint8_t)(INFO))

enum RtT_Types
{
	R_386_NONE = 0,			// No relocation
	R_386_32 = 1,			// Symbol + Offset
	R_386_PC32 = 2,			// Symbol + Offset - Section Offset
	R_386_RELATIVE = 8,
};

struct Elf32_Phdr
{
	Elf32_Word		p_type;
	Elf32_Off		p_offset;
	Elf32_Addr		p_vaddr;
	Elf32_Addr		p_paddr;
	Elf32_Word		p_filesz;
	Elf32_Word		p_memsz;
	Elf32_Word		p_flags;
	Elf32_Word		p_align;
};

enum PH_Types
{
	PHT_NULL = 0,
	PHT_LOAD = 1,
	PHT_DYNAMIC = 2,
	PHT_INTERP = 3,
	PHT_NOTE = 4,
	PHT_SHLIB = 5,
	PHT_PHDIR = 6,
	PHT_LOOS = 0x60000000,
	PHT_HIOS = 0x6FFFFFFF,
	PHT_LOPROC = 0x70000000,
	PHT_HIPROC = 0x7FFFFFFF
};

#define ELF32_R_SYM(INFO)		((INFO) >> 8)
#define ELF32_R_TYPE(INFO)		((uint8_t)(INFO))

#define DO_386_32(S, A)			((S) + (A))
#define DO_386_RELATIVE(B, A)	((B) + (A))
#define DO_386_PC32(S, A, P)	((S) + (A) - (P))

#define PF_X	1
#define PF_W	2
#define PF_R	4
