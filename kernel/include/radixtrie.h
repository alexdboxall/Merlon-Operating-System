#pragma once

#include <common.h>
#include <stdint.h>

#define DATA_BYTES_IN_LONG      31

struct long_bool_list {
    uint8_t data[DATA_BYTES_IN_LONG];
    uint8_t length;
};

struct radix_trie;

struct radix_trie* RadixTrieCreate(void);
void RadixTrieInsert(struct radix_trie* trie, const struct long_bool_list* key, void* value);
void* RadixTrieGet(struct radix_trie* trie, const struct long_bool_list* key);

struct long_bool_list RadixTrieCreateBoolListFromData(uint8_t* data, int num_bytes);
struct long_bool_list RadixTrieCreateBoolListFromData64(char* data);