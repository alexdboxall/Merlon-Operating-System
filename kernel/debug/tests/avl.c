
#include <debug.h>
#include <assert.h>
#include <panic.h>
#include <string.h>
#include <log.h>
#include <tree.h>
#include <heap.h>
#include <physical.h>

#ifndef NDEBUG

/*
typedef void (*tree_deletion_handler)(void*);
typedef int (*tree_comparator)(void*, void*);

void* TreeGet(struct tree* tree, void* data);
struct tree_node* TreeGetRootNode(struct tree* tree);
struct tree_node* TreeGetLeft(struct tree_node* node);
struct tree_node* TreeGetRight(struct tree_node* node);
void* TreeGetData(struct tree_node* node);
tree_deletion_handler TreeSetDeletionHandler(struct tree* tree, tree_deletion_handler handler);
tree_comparator TreeSetComparator(struct tree* tree, tree_comparator comparator);*/

TFW_CREATE_TEST(AVLTreeBasic) { TFW_IGNORE_UNUSED
    int heap_allocations = DbgGetOutstandingHeapAllocations();
    struct tree* tree = TreeCreate();
    assert(TreeSize(tree) == 0);
    TreeInsert(tree, (void*) 5);
    assert(TreeSize(tree) == 1);
    assert(TreeContains(tree, (void*) 5));
    assert(!TreeContains(tree, (void*) 7));
    assert(TreeGet(tree, (void*) 5) == (void*) 5);
    TreeInsert(tree, (void*) 7);
    TreeInsert(tree, (void*) 9);
    TreeInsert(tree, (void*) 11);
    TreeInsert(tree, (void*) 8);
    TreeInsert(tree, (void*) 6);
    assert(TreeSize(tree) == 6);
    TreeInsert(tree, (void*) 4);
    assert(TreeSize(tree) == 7);
    TreeDelete(tree, (void*) 5);
    assert(TreeSize(tree) == 6);
    assert(TreeContains(tree, (void*) 4));
    assert(!TreeContains(tree, (void*) 5));
    assert(TreeContains(tree, (void*) 6));
    assert(TreeContains(tree, (void*) 7));
    TreeDestroy(tree);

    /*
     * Ensure there aren't any memory leaks.
     */
    assert(DbgGetOutstandingHeapAllocations() == heap_allocations);
}


void RegisterTfwAVLTreeTests(void) {
    RegisterTfwTest("AVL trees (basic tests)", TFW_SP_AFTER_HEAP, AVLTreeBasic, PANIC_UNIT_TEST_OK, 0);
}

#endif

