
#include <debug.h>
#include <assert.h>
#include <panic.h>
#include <string.h>
#include <log.h>
#include <avl.h>
#include <heap.h>
#include <physical.h>

#ifndef NDEBUG

/*
typedef void (*avl_deletion_handler)(void*);
typedef int (*avl_comparator)(void*, void*);

void* AvlTreeGet(struct avl_tree* tree, void* data);
struct avl_node* AvlTreeGetRootNode(struct avl_tree* tree);
struct avl_node* AvlTreeGetLeft(struct avl_node* node);
struct avl_node* AvlTreeGetRight(struct avl_node* node);
void* AvlTreeGetData(struct avl_node* node);
avl_deletion_handler AvlTreeSetDeletionHandler(struct avl_tree* tree, avl_deletion_handler handler);
avl_comparator AvlTreeSetComparator(struct avl_tree* tree, avl_comparator comparator);*/

TFW_CREATE_TEST(AVLTreeBasic) { TFW_IGNORE_UNUSED
    int heap_allocations = DbgGetOutstandingHeapAllocations();
    struct avl_tree* tree = AvlTreeCreate();
    assert(AvlTreeSize(tree) == 0);
    AvlTreeInsert(tree, (void*) 5);
    assert(AvlTreeSize(tree) == 1);
    assert(AvlTreeContains(tree, (void*) 5));
    assert(!AvlTreeContains(tree, (void*) 7));
    assert(AvlTreeGet(tree, (void*) 5) == (void*) 5);
    AvlTreeInsert(tree, (void*) 7);
    AvlTreeInsert(tree, (void*) 9);
    AvlTreeInsert(tree, (void*) 11);
    AvlTreeInsert(tree, (void*) 8);
    AvlTreeInsert(tree, (void*) 6);
    assert(AvlTreeSize(tree) == 6);
    AvlTreeInsert(tree, (void*) 4);
    assert(AvlTreeSize(tree) == 7);
    AvlTreeDelete(tree, (void*) 5);
    assert(AvlTreeSize(tree) == 6);
    assert(AvlTreeContains(tree, (void*) 4));
    assert(!AvlTreeContains(tree, (void*) 5));
    assert(AvlTreeContains(tree, (void*) 6));
    assert(AvlTreeContains(tree, (void*) 7));
    AvlTreeDestroy(tree);

    /*
     * Ensure there aren't any memory leaks.
     */
    assert(DbgGetOutstandingHeapAllocations() == heap_allocations);
}


void RegisterTfwAVLTreeTests(void) {
    RegisterTfwTest("AVL trees (basic tests)", TFW_SP_AFTER_HEAP, AVLTreeBasic, PANIC_UNIT_TEST_OK, 0);
}

#endif

