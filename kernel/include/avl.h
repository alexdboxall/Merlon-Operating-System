#pragma once

#include <stdbool.h>

struct avl_tree;
struct avl_node;

typedef void (*avl_deletion_handler)(void*);
typedef int (*avl_comparator)(void*, void*);

struct avl_tree* AvlTreeCreate(void);
void AvlTreeInsert(struct avl_tree* tree, void* data);
void AvlTreeDelete(struct avl_tree* tree, void* data);
bool AvlTreeContains(struct avl_tree* tree, void* data);
void* AvlTreeGet(struct avl_tree* tree, void* data);
int AvlTreeSize(struct avl_tree* tree);
void AvlTreeDestroy(struct avl_tree* tree);
struct avl_node* AvlTreeGetRootNode(struct avl_tree* tree);
struct avl_node* AvlTreeGetLeft(struct avl_node* node);
struct avl_node* AvlTreeGetRight(struct avl_node* node);
void* AvlTreeGetData(struct avl_node* node);
avl_deletion_handler AvlTreeSetDeletionHandler(struct avl_tree* tree, avl_deletion_handler handler);
avl_comparator AvlTreeSetComparator(struct avl_tree* tree, avl_comparator comparator);