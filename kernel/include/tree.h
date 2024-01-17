#pragma once

#include <common.h>

typedef void (*tree_deletion_handler)(void*);
typedef int (*tree_comparator)(void*, void*);

struct tree_node {
    struct tree_node* left;
    struct tree_node* right;
    void* data;
};

struct tree {
	int size;
	struct tree_node* root;
    tree_deletion_handler deletion_handler;
    tree_comparator equality_handler;
};

void TreePrint(struct tree* tree, void(*printer)(void*));
struct tree* TreeCreate(void);
void TreeInsert(struct tree* tree, void* data);
void TreeDelete(struct tree* tree, void* data);
bool TreeContains(struct tree* tree, void* data);
void* TreeGet(struct tree* tree, void* data);
int TreeSize(struct tree* tree);
void TreeDestroy(struct tree* tree);
tree_deletion_handler TreeSetDeletionHandler(struct tree* tree, tree_deletion_handler handler);
tree_comparator TreeSetComparator(struct tree* tree, tree_comparator comparator);