
#include <debug.h>
#include <assert.h>
#include <panic.h>
#include <string.h>
#include <log.h>
#include <arch.h>
#include <physical.h>
#include <_partial_/stdlib.h>

#ifndef NDEBUG

TFW_CREATE_TEST(IsPageAligned) { TFW_IGNORE_UNUSED
    assert(AllocPhys() % ARCH_PAGE_SIZE == 0);
}

TFW_CREATE_TEST(DeallocationChecksForPageAlignment) { TFW_IGNORE_UNUSED
    size_t p = AllocPhys();
    DeallocPhys(p + context);
}

TFW_CREATE_TEST(DoubleDeallocationFails) { TFW_IGNORE_UNUSED
    size_t p = AllocPhys();
    DeallocPhys(p);
    DeallocPhys(p);
}

TFW_CREATE_TEST(SanityCheck) { TFW_IGNORE_UNUSED 
    AllocPhys();
    AllocPhys();
}

TFW_CREATE_TEST(BasicAllocationTest) { TFW_IGNORE_UNUSED 
    size_t a = AllocPhys();
    size_t b = AllocPhys();
    size_t c = AllocPhys();
    assert(a != b);
    assert(a != c);
    assert(b != c);
}

TFW_CREATE_TEST(BasicDeallocationTest) { TFW_IGNORE_UNUSED 
    size_t a = AllocPhys();
    size_t b = AllocPhys();
    DeallocPhys(a);
    DeallocPhys(b);
    DeallocPhys(AllocPhys());
}

TFW_CREATE_TEST(StressTest) { TFW_IGNORE_UNUSED
    size_t frames[512];
    memset(frames, 0, sizeof(frames));
    int allocated = 0;

    srand(context * 1234 + 12);
    
    // context 0: 40,000                    (should be around 200ms)
    // context 1: 250,000                   (should be around 1s)
    // context 2: 3,640,000 (NIGHTLY)       (should be around 18s)
    // context 3: 24,070,000 (NIGHTLY)      (should be around 120s)

    int limit = 10000 * (context * context * context * 3 + 2) * (context * context * 3 + 2);
    for (int i = 0; i < limit; ++i) {
        /*
         * This all gets a bit dodgy if it's too much higher than 400 on a 4MB system, as it will
         * eventually need to evict pages, but because we haven't actually mapped any of them into
         * virtual memory, we just hang.
         */
        if (allocated < (300 + (rand() % 80))) {
            size_t f = AllocPhys();
            for (int j = 0; j < 512; ++j) {
                if (frames[j] == f) {
                    Panic(PANIC_MANUALLY_INITIATED);
                }
            }
            for (int j = 0; j < 512; ++j) {
                if (frames[j] == 0) {
                    ++allocated;
                    frames[j] = f;
                    break;
                }
            }
        } else {
            while (allocated > 0) {
                for (int j = 0; j < 512; ++j) {
                    int k = rand() % 256;
                    if (frames[k] != 0) {
                        DeallocPhys(frames[k]);
                        --allocated;
                        frames[k] = 0;
                        break;
                    }   
                }
                if (rand() % 17 == 0) {
                    break;
                }
            }
        }
    }
}

TFW_CREATE_TEST(ContiguousAllocationRequiresStackAllocator) { TFW_IGNORE_UNUSED 
    assert(AllocPhysContiguous(ARCH_PAGE_SIZE, 0, 0, 0) == 0);
}

void RegisterTfwPhysTests(void) {
    RegisterTfwTest("Is AllocPhys sane", TFW_SP_AFTER_PHYS, SanityCheck, PANIC_UNIT_TEST_OK, 0);
    RegisterTfwTest("Basic AllocPhys test (bitmap)", TFW_SP_AFTER_PHYS, BasicAllocationTest, PANIC_UNIT_TEST_OK, 0);
    RegisterTfwTest("Basic AllocPhys test (stack)", TFW_SP_AFTER_PHYS_REINIT, BasicAllocationTest, PANIC_UNIT_TEST_OK, 0);
    RegisterTfwTest("Basic DeallocPhys test (bitmap)", TFW_SP_AFTER_PHYS, BasicDeallocationTest, PANIC_UNIT_TEST_OK, 0);
    RegisterTfwTest("Basic DeallocPhys test (stack)", TFW_SP_AFTER_PHYS_REINIT, BasicDeallocationTest, PANIC_UNIT_TEST_OK, 0);
    RegisterTfwTest("AllocPhys and DeallocPhys stress test (bitmap 1)", TFW_SP_AFTER_PHYS, StressTest, PANIC_UNIT_TEST_OK, 0);
    RegisterNightlyTfwTest("AllocPhys and DeallocPhys stress test (bitmap 2)", TFW_SP_AFTER_PHYS, StressTest, PANIC_UNIT_TEST_OK, 1);
    RegisterTfwTest("AllocPhys and DeallocPhys stress test (stack 1)", TFW_SP_AFTER_PHYS_REINIT, StressTest, PANIC_UNIT_TEST_OK, 0);
    RegisterTfwTest("AllocPhys and DeallocPhys stress test (stack 2)", TFW_SP_AFTER_PHYS_REINIT, StressTest, PANIC_UNIT_TEST_OK, 1);
    RegisterNightlyTfwTest("AllocPhys and DeallocPhys stress test (stack 3)", TFW_SP_AFTER_PHYS_REINIT, StressTest, PANIC_UNIT_TEST_OK, 2);
    RegisterNightlyTfwTest("AllocPhys and DeallocPhys stress test (stack 4)", TFW_SP_AFTER_PHYS_REINIT, StressTest, PANIC_UNIT_TEST_OK, 3);
    RegisterTfwTest("AllocPhys returns page aligned addresses", TFW_SP_AFTER_HEAP, IsPageAligned, PANIC_UNIT_TEST_OK, 0);
    RegisterTfwTest("DeallocPhys only accepts page aligned addresses (1)", TFW_SP_AFTER_HEAP, DeallocationChecksForPageAlignment, PANIC_ASSERTION_FAILURE, 1);
    RegisterTfwTest("DeallocPhys only accepts page aligned addresses (2)", TFW_SP_AFTER_HEAP, DeallocationChecksForPageAlignment, PANIC_ASSERTION_FAILURE, ARCH_PAGE_SIZE / 2);
    RegisterTfwTest("DeallocPhys checks for double allocation", TFW_SP_AFTER_HEAP, DoubleDeallocationFails, PANIC_ASSERTION_FAILURE, 0);

    RegisterTfwTest("AllocPhysContiguous requires the stack allocator", TFW_SP_AFTER_PHYS, ContiguousAllocationRequiresStackAllocator, PANIC_UNIT_TEST_OK, 0);

    // TODO: contiguous tests...
}

#endif
