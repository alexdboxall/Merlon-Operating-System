
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
    struct priority_queue* queue = PriorityQueueCreate(100, max, 4);
    assert(PriorityQueueGetCapacity(queue) == 100);
    int elem;
    for (int i = 0; i < 100; ++i) {
        elem = i;
        PriorityQueueInsert(queue, &elem, i * 3);
        assert(PriorityQueueGetUsedSize(queue) == i + 1);
    }

    for (int i = 0; i < 100; ++i) {
        struct priority_queue_result res = PriorityQueuePeek(queue);
        int* d = res.data;
        if (max) {
            assert(*d == 99 - i);
            assert(res.priority == (99 - i) * 3);
        } else {
            assert(*d == i);
            assert(res.priority == i * 3);
        }
        assert(PriorityQueueGetUsedSize(queue) == 100 - i);
        PriorityQueuePop(queue);
        assert(PriorityQueueGetUsedSize(queue) == 99 - i);
    }
    PriorityQueueDestroy(queue);
}

TFW_CREATE_TEST(PriorityQueueCombined) { TFW_IGNORE_UNUSED
    PQInsertionAndDeletionTest(true);
    PQInsertionAndDeletionTest(false);
}

TFW_CREATE_TEST(PriorityQueueStress) { TFW_IGNORE_UNUSED
    int expected_size = 0;

    srand(1);

    struct priority_queue* queue = PriorityQueueCreate(1000, true, 8);
    assert(PriorityQueueGetCapacity(queue) == 1000);

    uint32_t data[2];

    for (int i = 0; i < 1500000; ++i) {
        int rng = rand();
        for (int j = 0; j < 2; ++j) {
            data[j] = rand();
        }

        if (rng % 3 && expected_size < 1000) {
            PriorityQueueInsert(queue, data, rng % 10000);
            ++expected_size;
            
        } else if (rng % 3 == 0 && expected_size > 1) {
            struct priority_queue_result r1 = PriorityQueuePeek(queue);
            PriorityQueuePop(queue);
            struct priority_queue_result r2 = PriorityQueuePeek(queue);
            PriorityQueuePop(queue);
            assert(r1.priority >= r2.priority);
            expected_size -= 2;

        } else {
            --i;
        }

        assert(PriorityQueueGetUsedSize(queue) == expected_size);
    }

    int prev = 99999999;
    while (PriorityQueueGetUsedSize(queue) > 0) {
        struct priority_queue_result r1 = PriorityQueuePeek(queue);
        PriorityQueuePop(queue);
        assert(r1.priority <= prev);
        prev = r1.priority;
    }

    PriorityQueueDestroy(queue);
}

TFW_CREATE_TEST(PriorityQueueInsertWhenFill) { TFW_IGNORE_UNUSED
    int i = 0;
    struct priority_queue* queue = PriorityQueueCreate(1, true, 4);
    PriorityQueueInsert(queue, &i, 0);
    PriorityQueueInsert(queue, &i, 0);
}


TFW_CREATE_TEST(PriorityQueuePeekWhenEmpty) { TFW_IGNORE_UNUSED
    PriorityQueuePeek(PriorityQueueCreate(1, true, 4));
}

TFW_CREATE_TEST(PriorityQueuePopWhenEmpty) { TFW_IGNORE_UNUSED
    PriorityQueuePop(PriorityQueueCreate(1, true, 4));
}

TFW_CREATE_TEST(PriorityQueueStangeSizes1) { TFW_IGNORE_UNUSED
    PriorityQueueCreate(0, true, 4);
}

TFW_CREATE_TEST(PriorityQueueStangeSizes2) { TFW_IGNORE_UNUSED
    PriorityQueueCreate(-1, true, 4);
}

TFW_CREATE_TEST(PriorityQueueStangeSizes3) { TFW_IGNORE_UNUSED
    PriorityQueueCreate(1, true, 0);
}

TFW_CREATE_TEST(PriorityQueueStangeSizes4) { TFW_IGNORE_UNUSED
    PriorityQueueCreate(1, true, -5);
}

void RegisterTfwPriorityQueueTests(void) {
    RegisterTfwTest("Priority queues (general tests)", TFW_SP_AFTER_HEAP, PriorityQueueCombined, PANIC_UNIT_TEST_OK, 0);
    RegisterTfwTest("Priority queues (stress tests)", TFW_SP_AFTER_HEAP, PriorityQueueStress, PANIC_UNIT_TEST_OK, 0);
    RegisterTfwTest("Priority queues (insert when full)", TFW_SP_AFTER_HEAP, PriorityQueueInsertWhenFill, PANIC_PRIORITY_QUEUE, 0);
    RegisterTfwTest("Priority queues (peek when empty)", TFW_SP_AFTER_HEAP, PriorityQueuePeekWhenEmpty, PANIC_PRIORITY_QUEUE, 0);
    RegisterTfwTest("Priority queues (pop when empty)", TFW_SP_AFTER_HEAP, PriorityQueuePopWhenEmpty, PANIC_PRIORITY_QUEUE, 0);
    RegisterTfwTest("Priority queues (strange sizes, 1)", TFW_SP_AFTER_HEAP, PriorityQueueStangeSizes1, PANIC_ASSERTION_FAILURE, 0);
    RegisterTfwTest("Priority queues (strange sizes, 2)", TFW_SP_AFTER_HEAP, PriorityQueueStangeSizes2, PANIC_ASSERTION_FAILURE, 0);
    RegisterTfwTest("Priority queues (strange sizes, 3)", TFW_SP_AFTER_HEAP, PriorityQueueStangeSizes3, PANIC_ASSERTION_FAILURE, 0);
    RegisterTfwTest("Priority queues (strange sizes, 4)", TFW_SP_AFTER_HEAP, PriorityQueueStangeSizes4, PANIC_ASSERTION_FAILURE, 0);
}

#endif

