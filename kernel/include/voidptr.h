#pragma once
#include <stdint.h>

#define AddVoidPtr(ptr, offset) ((void*) (((uint8_t*) ptr) + offset))
#define SubVoidPtr(ptr, offset) ((void*) (((uint8_t*) ptr) - offset))
