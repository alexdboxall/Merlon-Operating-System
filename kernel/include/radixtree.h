#pragma once

#include <common.h>

struct radix_tree;

struct radix_tree* RadixTreeCreate(bool paging_allowed);
void* RadixTreeGet(struct radix_tree* tree);
void RadixTreeInsert(struct radix_tree* tree, uint8_t* key, int key_length, void* data);