#pragma once

#include <common.h>

void InitProgramLoader(void);
int LoadProgramLoaderIntoAddressSpace(size_t* entry_point_out); 