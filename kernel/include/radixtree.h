#pragma once

#include <common.h>

struct radix_tree;

export struct radix_tree* RadixTreeCreate(bool paging_allowed);
export void* RadixTreeGet(struct radix_tree* tree);
export void RadixTreeInsert(struct radix_tree* tree, uint8_t* key, int key_length, void* data);