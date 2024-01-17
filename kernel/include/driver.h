#pragma once

#include <common.h>

struct rel {
	size_t address;
	size_t value;
};

struct rel_table {
	int total_entries; 
	int used_entries;
	struct rel* entries;
};

struct vas;

void InitSymbolTable(void);
int RequireDriver(const char* name);
size_t GetDriverAddress(const char* name);
size_t GetSymbolAddress(const char* symbol);
void AddSymbol(const char* symbol, size_t address);

void SortRelocationTable(struct rel_table* table);
void AddToRelocationTable(struct rel_table* table, size_t addr, size_t val);
struct rel_table* CreateRelocationTable(int count);
void RelocatePage(struct vas*, size_t relocation_base, size_t virt);