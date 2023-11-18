
#include <debug.h>
#include <assert.h>
#include <panic.h>
#include <string.h>
#include <log.h>
#include <arch.h>
#include <irql.h>

#ifndef NDEBUG

static int counter = 0;

static void defer_me(void* context) {
    counter = (size_t) context;
}

static void defer_me_2(void* context) {
    counter += (size_t) context;
}

static void defer_me_3(void* context) {
    int irql = RaiseIrql(IRQL_HIGH);
    LowerIrql(irql);

    counter += (size_t) context;
}

static void internal_def4_1(void* context) {
    (void) context;

    assert(counter == 20);
    counter = 30;
}

static void internal_def4_2(void* context) {
    (void) context;

    assert(counter == 40);
    counter = 50;
}

static void defer_me_4(void* context) {
    (void) context;

    assert(counter == 0);

    int irql = RaiseIrql(IRQL_HIGH);
    DeferUntilIrql(IRQL_TIMER, internal_def4_1, NULL);
    DeferUntilIrql(IRQL_PAGE_FAULT, internal_def4_2, NULL);
    counter = 20;
    LowerIrql(irql);
    assert(counter == 30);
}

static void defer_me_5(void* context) {
    (void) context;

    assert(counter == 30);
    counter = 40;
}

static void defer_me_6(void* context) {
    (void) context;
    int v = (size_t) context;
    assert(counter == v - 1);
    counter = v;
}

TFW_CREATE_TEST(RaiseLowerTest) { TFW_IGNORE_UNUSED
    int irql_a = GetIrql();
    assert(irql_a == IRQL_STANDARD);
    int irql_b = RaiseIrql(IRQL_SCHEDULER);
    assert(irql_a == irql_b);
    assert(GetIrql() == IRQL_SCHEDULER);
    int irql_c = RaiseIrql(IRQL_HIGH);
    assert(irql_c == IRQL_SCHEDULER);
    assert(GetIrql() == IRQL_HIGH);
    LowerIrql(irql_c);
    assert(GetIrql() == IRQL_SCHEDULER);
    LowerIrql(irql_a);
    assert(GetIrql() == irql_a);
}

TFW_CREATE_TEST(DeferRunsImmediatelyAtLevel) { TFW_IGNORE_UNUSED
    EXACT_IRQL(IRQL_STANDARD);
    counter = 0;
    DeferUntilIrql(IRQL_STANDARD, defer_me, (void*) 1);
    assert(counter == 1);
    RaiseIrql(IRQL_SCHEDULER);
    DeferUntilIrql(IRQL_SCHEDULER, defer_me, (void*) 2);
    assert(counter == 2);
    RaiseIrql(IRQL_HIGH);
    DeferUntilIrql(IRQL_HIGH, defer_me, (void*) 3);
    assert(counter == 3);
}

TFW_CREATE_TEST(DeferDoesntWorkBeforeHeap) { TFW_IGNORE_UNUSED
    EXACT_IRQL(IRQL_STANDARD);
    counter = 0;
    DeferUntilIrql(IRQL_STANDARD, defer_me, (void*) 1);
    assert(counter == 1);
    RaiseIrql(IRQL_SCHEDULER);
    DeferUntilIrql(IRQL_STANDARD, defer_me, (void*) 2);
    assert(counter == 1);
    LowerIrql(IRQL_SCHEDULER);
    assert(counter == 1);
}

TFW_CREATE_TEST(DeferWorksNormally) { TFW_IGNORE_UNUSED
    EXACT_IRQL(IRQL_STANDARD);
    counter = 0;
    DeferUntilIrql(IRQL_STANDARD, defer_me, (void*) 1);
    assert(counter == 1);
    RaiseIrql(IRQL_SCHEDULER);
    DeferUntilIrql(IRQL_STANDARD, defer_me, (void*) 2);
    assert(counter == 1);
    LowerIrql(IRQL_STANDARD);
    assert(counter == 2);
}

TFW_CREATE_TEST(DeferWorksThroughLevelsInOrder) { TFW_IGNORE_UNUSED
    EXACT_IRQL(IRQL_STANDARD);
    counter = 0;

    RaiseIrql(IRQL_HIGH);
    DeferUntilIrql(IRQL_HIGH, defer_me_6, (void*) 1);
    assert(counter == 1);
    DeferUntilIrql(IRQL_TIMER, defer_me_6, (void*) 2);
    assert(counter == 1);
    DeferUntilIrql(IRQL_DRIVER, defer_me_6, (void*) 3);
    assert(counter == 1);
    DeferUntilIrql(IRQL_SCHEDULER, defer_me_6, (void*) 4);
    assert(counter == 1);
    DeferUntilIrql(IRQL_PAGE_FAULT, defer_me_6, (void*) 5);
    assert(counter == 1);
    DeferUntilIrql(IRQL_STANDARD, defer_me_6, (void*) 6);
    assert(counter == 1);

    LowerIrql(IRQL_STANDARD);
    assert(counter == 6);
}


TFW_CREATE_TEST(DeferWorksThroughLevels) { TFW_IGNORE_UNUSED
    EXACT_IRQL(IRQL_STANDARD);
    counter = 0;

    RaiseIrql(IRQL_HIGH);
    DeferUntilIrql(IRQL_HIGH, defer_me_2, (void*) 1);
    assert(counter == 1);
    DeferUntilIrql(IRQL_TIMER, defer_me_2, (void*) 2);
    assert(counter == 1);
    DeferUntilIrql(IRQL_DRIVER, defer_me_2, (void*) 4);
    assert(counter == 1);
    DeferUntilIrql(IRQL_SCHEDULER, defer_me_2, (void*) 8);
    assert(counter == 1);
    DeferUntilIrql(IRQL_PAGE_FAULT, defer_me_2, (void*) 16);
    assert(counter == 1);
    DeferUntilIrql(IRQL_STANDARD, defer_me_2, (void*) 32);
    assert(counter == 1);

    LowerIrql(IRQL_STANDARD);
    assert(counter == 63);
}

TFW_CREATE_TEST(DeferMultipleAtSameLevel) { TFW_IGNORE_UNUSED
    EXACT_IRQL(IRQL_STANDARD);
    counter = 0;

    RaiseIrql(IRQL_HIGH);
    DeferUntilIrql(IRQL_TIMER, defer_me_2, (void*) 1);
    assert(counter == 0);
    DeferUntilIrql(IRQL_TIMER, defer_me_2, (void*) 2);
    assert(counter == 0);
    DeferUntilIrql(IRQL_TIMER, defer_me_2, (void*) 4);
    assert(counter == 0);

    LowerIrql(IRQL_STANDARD);
    assert(counter == 7);
}

TFW_CREATE_TEST(DeferDoesntRunLowHandlers) { TFW_IGNORE_UNUSED
    EXACT_IRQL(IRQL_STANDARD);
    counter = 0;

    RaiseIrql(IRQL_HIGH);
    DeferUntilIrql(IRQL_HIGH, defer_me_2, (void*) 1);
    assert(counter == 1);
    DeferUntilIrql(IRQL_TIMER, defer_me_2, (void*) 2);
    assert(counter == 1);
    DeferUntilIrql(IRQL_DRIVER, defer_me_2, (void*) 4);
    assert(counter == 1);
    DeferUntilIrql(IRQL_SCHEDULER, defer_me_2, (void*) 8);
    assert(counter == 1);
    DeferUntilIrql(IRQL_PAGE_FAULT, defer_me_2, (void*) 16);
    assert(counter == 1);
    DeferUntilIrql(IRQL_STANDARD, defer_me_2, (void*) 32);
    assert(counter == 1);

    LowerIrql(IRQL_SCHEDULER);
    assert(counter == 15);
}

TFW_CREATE_TEST(DeferWorksThroughLevelsStepping) { TFW_IGNORE_UNUSED
    EXACT_IRQL(IRQL_STANDARD);
    counter = 0;

    RaiseIrql(IRQL_HIGH);
    DeferUntilIrql(IRQL_HIGH, defer_me_2, (void*) 1);
    assert(counter == 1);
    DeferUntilIrql(IRQL_TIMER, defer_me_2, (void*) 2);
    assert(counter == 1);
    DeferUntilIrql(IRQL_TIMER, defer_me_2, (void*) 4);
    assert(counter == 1);
    DeferUntilIrql(IRQL_DRIVER, defer_me_2, (void*) 8);
    assert(counter == 1);
    DeferUntilIrql(IRQL_SCHEDULER, defer_me_2, (void*) 16);
    assert(counter == 1);
    DeferUntilIrql(IRQL_PAGE_FAULT, defer_me_2, (void*) 32);
    assert(counter == 1);
    DeferUntilIrql(IRQL_STANDARD, defer_me_2, (void*) 64);
    assert(counter == 1);

    LowerIrql(IRQL_DRIVER);
    assert(counter == 15);
    RaiseIrql(IRQL_HIGH);
    LowerIrql(IRQL_DRIVER);
    assert(counter == 15);
    LowerIrql(IRQL_PAGE_FAULT);
    assert(counter == 63);
    LowerIrql(IRQL_STANDARD);
    assert(counter == 127);
}

TFW_CREATE_TEST(DeferWithLoweringInHandler) { TFW_IGNORE_UNUSED
    EXACT_IRQL(IRQL_STANDARD);
    counter = 0;

    RaiseIrql(IRQL_SCHEDULER);
    DeferUntilIrql(IRQL_PAGE_FAULT, defer_me_3, (void*) 55);
    assert(counter == 0);
    LowerIrql(IRQL_STANDARD);
    assert(counter == 55);
}

TFW_CREATE_TEST(DeferWithDeferringInHandler) { TFW_IGNORE_UNUSED
    EXACT_IRQL(IRQL_STANDARD);
    counter = 0;

    RaiseIrql(IRQL_HIGH);
    DeferUntilIrql(IRQL_DRIVER, defer_me_4, NULL);
    DeferUntilIrql(IRQL_SCHEDULER, defer_me_5, NULL);
    assert(counter == 0);
    LowerIrql(IRQL_STANDARD);
    assert(counter == 50);
}

void RegisterTfwIrqlTests(void) {
    RegisterTfwTest("RaiseIrql, LowerIrql and GetIrql work", TFW_SP_AFTER_HEAP, RaiseLowerTest, PANIC_UNIT_TEST_OK, 0);
    RegisterTfwTest("DeferUntilIrql gets run immediately at level", TFW_SP_AFTER_HEAP, DeferRunsImmediatelyAtLevel, PANIC_UNIT_TEST_OK, 0);
    RegisterTfwTest("DeferUntilIrql gets run on level lowering", TFW_SP_AFTER_HEAP, DeferWorksNormally, PANIC_UNIT_TEST_OK, 0);
    RegisterTfwTest("DeferUntilIrql defers get ignored before heap", TFW_SP_AFTER_PHYS, DeferDoesntWorkBeforeHeap, PANIC_UNIT_TEST_OK, 0);
    RegisterTfwTest("DeferUntilIrql runs multiple handlers at same level", TFW_SP_AFTER_HEAP, DeferMultipleAtSameLevel, PANIC_UNIT_TEST_OK, 0);
    RegisterTfwTest("DeferUntilIrql runs multiple handlers at different levels (1)", TFW_SP_AFTER_HEAP, DeferWorksThroughLevels, PANIC_UNIT_TEST_OK, 0);
    RegisterTfwTest("DeferUntilIrql runs multiple handlers at different levels (2)", TFW_SP_AFTER_HEAP, DeferWorksThroughLevelsStepping, PANIC_UNIT_TEST_OK, 0);
    RegisterTfwTest("DeferUntilIrql runs multiple handlers in the correct order", TFW_SP_AFTER_HEAP, DeferWorksThroughLevelsInOrder, PANIC_UNIT_TEST_OK, 0);
    RegisterTfwTest("DeferUntilIrql doesn't run handlers below current level", TFW_SP_AFTER_HEAP, DeferDoesntRunLowHandlers, PANIC_UNIT_TEST_OK, 0);
    RegisterTfwTest("DeferUntilIrql when handler calls LowerIrql", TFW_SP_AFTER_HEAP, DeferWithLoweringInHandler, PANIC_UNIT_TEST_OK, 0);
    RegisterTfwTest("DeferUntilIrql when handler calls DeferUntilIrql", TFW_SP_AFTER_HEAP, DeferWithDeferringInHandler, PANIC_UNIT_TEST_OK, 0);
}

#endif
