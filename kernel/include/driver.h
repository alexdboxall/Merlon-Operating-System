#pragma once

#include <common.h>

struct relocation {
	size_t address;
	size_t value;
};

struct relocation_table {
	int total_entries; 
	int used_entries;
	struct relocation* entries;
};

struct vas;

void InitSymbolTable(void);
int RequireDriver(const char* name);
size_t GetDriverAddress(const char* name);
size_t GetSymbolAddress(const char* symbol);
void AddSymbol(const char* symbol, size_t address);

void SortRelocationTable(struct relocation_table* table);
void AddToRelocationTable(struct relocation_table* table, size_t addr, size_t val);
struct relocation_table* CreateRelocationTable(int count);
void RelocatePage(struct vas*, size_t relocation_base, size_t virt);