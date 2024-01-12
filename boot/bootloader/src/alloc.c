#include <bootloader.h>
#include <util.h>
#include <alloc.h>

void* AllocHeap(size_t size) {
    (void) size;
    return NULL;
}

void* AllocForModule(size_t size) {
    (void) size;
    return NULL;
}