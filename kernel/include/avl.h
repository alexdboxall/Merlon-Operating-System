#pragma once

#include <common.h>

struct avl_tree;
struct avl_node;

typedef void (*avl_deletion_handler)(void*);
typedef int (*avl_comparator)(void*, void*);

void AvlTreePrint(struct avl_tree* tree, void(*printer)(void*));
export struct avl_tree* AvlTreeCreate(void);
export void AvlTreeInsert(struct avl_tree* tree, void* data);
export void AvlTreeDelete(struct avl_tree* tree, void* data);
export bool AvlTreeContains(struct avl_tree* tree, void* data);
export void* AvlTreeGet(struct avl_tree* tree, void* data);
export int AvlTreeSize(struct avl_tree* tree);
export void AvlTreeDestroy(struct avl_tree* tree);
export struct avl_node* AvlTreeGetRootNode(struct avl_tree* tree);
export struct avl_node* AvlTreeGetLeft(struct avl_node* node);
export struct avl_node* AvlTreeGetRight(struct avl_node* node);
export void* AvlTreeGetData(struct avl_node* node);
export avl_deletion_handler AvlTreeSetDeletionHandler(struct avl_tree* tree, avl_deletion_handler handler);
export avl_comparator AvlTreeSetComparator(struct avl_tree* tree, avl_comparator comparator);