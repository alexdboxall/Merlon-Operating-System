
/*
 * adt/hashmap.c - String to Pointer Hashmaps
 *
 * Implements a hashmap which takes a string key, and pointer value.
 */

#include <heap.h>
#include <string.h>
#include <errno.h>
#include <log.h>
#include <panic.h>
#include <assert.h>
#include <common.h>
#include <linkedlist.h>

struct hashmap_node {
    char* key;
    void* value;
};

struct hashmap {
    struct linked_list** buckets;
    int num_buckets;
};

static uint32_t HashFunction(const char* str) {
    uint32_t hash = 0;
    for (int i = 0; str[i]; ++i) {
        hash = 31 * hash + str[i];
    }
    return hash;
}

struct hashmap* HashmapCreate(int buckets) {
    struct hashmap* map = AllocHeap(sizeof(struct hashmap));
    map->num_buckets = buckets;
    map->buckets = AllocHeapZero(sizeof(struct linked_list*) * buckets);
    return map;
}

static struct hashmap_node* GetInternalNode(struct hashmap* map, const char* key) {
    uint32_t hash = HashFunction(key) % map->num_buckets;

    if (map->buckets[hash] == NULL) {
        return NULL;
    }

    struct linked_list_node* node = ListGetFirstNode(map->buckets[hash]);
    while (node != NULL) {
        struct hashmap_node* inner = ListGetDataFromNode(node);

        if (!strcmp(inner->key, key)) {
            return inner;
        }

        node = ListGetNextNode(node);
    }

    return NULL;
}

bool HashmapContains(struct hashmap* map, const char* key) {
    return GetInternalNode(map, key) != NULL;
}

void* HashmapGet(struct hashmap* map, const char* key) {
    struct hashmap_node* inner = GetInternalNode(map, key);
    if (inner == NULL) {
        return NULL;
    } else {
        return inner->value;
    }
}

void HashmapSet(struct hashmap* map, const char* key, void* value) {
    struct hashmap_node* inner = GetInternalNode(map, key);
    if (inner == NULL) {
        struct hashmap_node* inner = AllocHeap(sizeof(struct hashmap_node));
        inner->key = strdup(key);
        inner->value = value;

        uint32_t hash = HashFunction(key) % map->num_buckets;
        if (map->buckets[hash] == NULL) {
            map->buckets[hash] = ListCreate();
        }
        
        ListInsertEnd(map->buckets[hash], inner);

    } else {
        inner->value = value;
    }
}
