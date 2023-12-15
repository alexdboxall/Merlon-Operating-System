#pragma once

#include <common.h>

void InitSymbolTable(void);
int RequireDriver(const char* name);
size_t GetDriverAddress(const char* name);
size_t GetSymbolAddress(const char* symbol);
void AddSymbol(const char* symbol, size_t address);