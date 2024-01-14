
#include <debug.h>
#include <assert.h>
#include <panic.h>
#include <string.h>
#include <log.h>
#include <arch.h>
#include <priorityqueue.h>
#include <stdlib.h>

#ifndef NDEBUG

static void PQInsertionAndDeletionTest(bool max) {
    struct heap_adt* queue = HeapAdtCreate(100, max, 4);
    assert(HeapAdtGetCapacity(queue) == 100);
    int elem;
    for (int i = 0; i < 100; ++i) {
        elem = i;
        HeapAdtInsert(queue, &elem, i * 3);
        assert(HeapAdtGetUsedSize(queue) == i + 1);
    }

    for (int i = 0; i < 100; ++i) {
        struct heap_adt_result res = HeapAdtPeek(queue);
        int* d = res.data;
        if (max) {
            assert(*d == 99 - i);
            assert((int) res.priority == (99 - i) * 3);
        } else {
            assert(*d == i);
            assert((int) res.priority == i * 3);
        }
        assert((int) HeapAdtGetUsedSize(queue) == 100 - i);
        HeapAdtPop(queue);
        assert((int) HeapAdtGetUsedSize(queue) == 99 - i);
    }
    HeapAdtDestroy(queue);
}

TFW_CREATE_TEST(HeapAdtCombined) { TFW_IGNORE_UNUSED
    PQInsertionAndDeletionTest(true);
    PQInsertionAndDeletionTest(false);
}

TFW_CREATE_TEST(HeapAdtStress) { TFW_IGNORE_UNUSED
    int expected_size = 0;

    srand(1);

    struct heap_adt* queue = HeapAdtCreate(100, true, 8);
    assert(HeapAdtGetCapacity(queue) == 100);

    uint32_t data[2];

    for (int i = 0; i < 1500000; ++i) {
        int rng = rand();
        for (int j = 0; j < 2; ++j) {
            data[j] = rand();
        }

        if (rng % 3 && expected_size < 100) {
            HeapAdtInsert(queue, data, rng % 10000);
            ++expected_size;
            
        } else if (rng % 3 == 0 && expected_size > 1) {
            struct heap_adt_result r1 = HeapAdtPeek(queue);
            HeapAdtPop(queue);
            struct heap_adt_result r2 = HeapAdtPeek(queue);
            HeapAdtPop(queue);
            assert(r1.priority >= r2.priority);
            expected_size -= 2;

        } else {
            --i;
        }

        assert(HeapAdtGetUsedSize(queue) == expected_size);
    }

    uint64_t prev = 99999999;
    while (HeapAdtGetUsedSize(queue) > 0) {
        struct heap_adt_result r1 = HeapAdtPeek(queue);
        HeapAdtPop(queue);
        assert(r1.priority <= prev);
        prev = r1.priority;
    }

    HeapAdtDestroy(queue);
}

TFW_CREATE_TEST(HeapAdtInsertWhenFill) { TFW_IGNORE_UNUSED
    int i = 0;
    struct heap_adt* queue = HeapAdtCreate(1, true, 4);
    HeapAdtInsert(queue, &i, 0);
    HeapAdtInsert(queue, &i, 0);
}


TFW_CREATE_TEST(HeapAdtPeekWhenEmpty) { TFW_IGNORE_UNUSED
    HeapAdtPeek(HeapAdtCreate(1, true, 4));
}

TFW_CREATE_TEST(HeapAdtPopWhenEmpty) { TFW_IGNORE_UNUSED
    HeapAdtPop(HeapAdtCreate(1, true, 4));
}

TFW_CREATE_TEST(HeapAdtStangeSizes1) { TFW_IGNORE_UNUSED
    HeapAdtCreate(0, true, 4);
}

TFW_CREATE_TEST(HeapAdtStangeSizes2) { TFW_IGNORE_UNUSED
    HeapAdtCreate(-1, true, 4);
}

TFW_CREATE_TEST(HeapAdtStangeSizes3) { TFW_IGNORE_UNUSED
    HeapAdtCreate(1, true, 0);
}

TFW_CREATE_TEST(HeapAdtStangeSizes4) { TFW_IGNORE_UNUSED
    HeapAdtCreate(1, true, -5);
}

void RegisterTfwHeapAdtTests(void) {
    RegisterTfwTest("Priority queues (general tests)", TFW_SP_AFTER_HEAP, HeapAdtCombined, PANIC_UNIT_TEST_OK, 0);
    RegisterTfwTest("Priority queues (stress tests)", TFW_SP_AFTER_HEAP, HeapAdtStress, PANIC_UNIT_TEST_OK, 0);
    RegisterTfwTest("Priority queues (insert when full)", TFW_SP_AFTER_HEAP, HeapAdtInsertWhenFill, PANIC_PRIORITY_QUEUE, 0);
    RegisterTfwTest("Priority queues (peek when empty)", TFW_SP_AFTER_HEAP, HeapAdtPeekWhenEmpty, PANIC_PRIORITY_QUEUE, 0);
    RegisterTfwTest("Priority queues (pop when empty)", TFW_SP_AFTER_HEAP, HeapAdtPopWhenEmpty, PANIC_PRIORITY_QUEUE, 0);
    RegisterTfwTest("Priority queues (strange sizes, 1)", TFW_SP_AFTER_HEAP, HeapAdtStangeSizes1, PANIC_ASSERTION_FAILURE, 0);
    RegisterTfwTest("Priority queues (strange sizes, 2)", TFW_SP_AFTER_HEAP, HeapAdtStangeSizes2, PANIC_ASSERTION_FAILURE, 0);
    RegisterTfwTest("Priority queues (strange sizes, 3)", TFW_SP_AFTER_HEAP, HeapAdtStangeSizes3, PANIC_ASSERTION_FAILURE, 0);
    RegisterTfwTest("Priority queues (strange sizes, 4)", TFW_SP_AFTER_HEAP, HeapAdtStangeSizes4, PANIC_ASSERTION_FAILURE, 0);
}

#endif

