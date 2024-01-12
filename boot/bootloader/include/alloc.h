#pragma once

#include <bootloader.h>
#include <common.h>

void* AllocHeap(size_t size);
void* AllocForModule(size_t size);