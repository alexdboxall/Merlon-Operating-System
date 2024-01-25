
#include <debug.h>
#include <assert.h>
#include <panic.h>
#include <string.h>
#include <log.h>
#include <tree.h>
#include <stdlib.h>
#include <heap.h>
#include <physical.h>

#ifndef NDEBUG

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

TFW_CREATE_TEST(AVLTreeStressInsert1) { TFW_IGNORE_UNUSED
    struct tree* tree = TreeCreate();
    for (int i = 10; i < 1000; i += 10) {
        TreeInsert(tree, (void*) i);
    }
    for (int i = 13; i < 1000; i += 10) {
        TreeInsert(tree, (void*) i);
    }
    for (int i = 18; i < 1000; i += 10) {
        TreeInsert(tree, (void*) i);
    }
    for (int i = 15; i < 1000; i += 10) {
        TreeInsert(tree, (void*) i);
    }

    for (int i = 10; i < 1000; i += 10) {
        assert(TreeContains(tree, (void*) i));
    }
    for (int i = 13; i < 1000; i += 10) {
        assert(TreeContains(tree, (void*) i));
    }
    for (int i = 18; i < 1000; i += 10) {
        assert(TreeContains(tree, (void*) i));
    }
    for (int i = 15; i < 1000; i += 10) {
        assert(TreeContains(tree, (void*) i));
    }

    for (int i = 11; i < 1000; i += 10) {
        assert(!TreeContains(tree, (void*) i));
    }
    for (int i = 12; i < 1000; i += 10) {
        assert(!TreeContains(tree, (void*) i));
    }
    for (int i = 14; i < 1000; i += 10) {
        assert(!TreeContains(tree, (void*) i));
    }
}

/*TFW_CREATE_TEST(AVLTreeStressInsert2) { TFW_IGNORE_UNUSED
    for (int i = 0; i < 20; ++i) {
        struct tree* tree = TreeCreate();
        for (int i = 0; i < 3000; ++i) {
            TreeInsert(tree, (void*) (rand()));
        }
        TreeDestroy(tree);
    }
}*/

void RegisterTfwAVLTreeTests(void) {
    RegisterTfwTest("AVL trees (basic tests)", TFW_SP_AFTER_HEAP, AVLTreeBasic, PANIC_UNIT_TEST_OK, 0);
    RegisterTfwTest("AVL trees (stress insert 1)", TFW_SP_AFTER_HEAP, AVLTreeStressInsert1, PANIC_UNIT_TEST_OK, 0);
    //RegisterTfwTest("AVL trees (stress insert 2)", TFW_SP_AFTER_HEAP, AVLTreeStressInsert2, PANIC_UNIT_TEST_OK, 0);
}

#endif

