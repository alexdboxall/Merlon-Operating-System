
#include <heap.h>
#include <assert.h>
#include <common.h>

struct radix_node {
    uint64_t key : 56;
    uint64_t length : 3;
    uint64_t terminal : 1;
    uint64_t : 4;

    struct radix_node* left;
    union {
        void* data;
        struct radix_node* right;
    };
};

struct radix_tree {
    struct radix_node* root;
    bool paging_allowed;
};

struct radix_tree* RadixTreeCreate(bool paging_allowed) {
    struct radix_tree* tree = AllocHeapEx(sizeof(struct radix_tree), paging_allowed ? HEAP_ALLOW_PAGING : HEAP_NO_FAULT);
    tree->root = NULL;
    tree->paging_allowed = paging_allowed;
    return tree;
}

void* RadixTreeGet(struct radix_tree* tree) {
    (void) tree;
    return NULL;
}

static struct radix_node* RadixNodeInsert(struct radix_tree* tree, struct radix_node* node, uint8_t* key, int key_length, void* data) {
    (void) tree;
    (void) node;
    (void) key;
    (void) key_length;
    (void) data;
    return NULL;
}

void RadixTreeInsert(struct radix_tree* tree, uint8_t* key, int key_length, void* data) {
    assert(tree != NULL);
    tree->root = RadixNodeInsert(tree, tree->root, key, key_length, data);
}