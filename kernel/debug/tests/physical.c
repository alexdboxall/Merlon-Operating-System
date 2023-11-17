
#include <debug.h>
#include <assert.h>
#include <panic.h>
#include <string.h>
#include <log.h>
#include <arch.h>
#include <physical.h>

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

    uint32_t next = 12 + context * 1234;
    
    // context 0: 40,000
    // context 1: 250,000
    // context 2: 3,640,000 (NIGHTLY)
    // context 3: 24,070,000 (NIGHTLY)

    int limit = 10000 * (context * context * context * 3 + 2) * (context * context * 3 + 2);
    for (int i = 0; i < limit; ++i) {
        int rng = (unsigned int) (next / 65536) % 32768;
        next = next * 1103515245 + 12345;

        if (allocated < (400 + (rng % 100))) {
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
                rng = (unsigned int) (next / 65536) % 32768;
                next = next * 1103515245 + 12345;
                for (int j = 0; j < 512; ++j) {
                    int k = (j + ((rng / 1234) % 256)) % 256;
                    if (frames[k] != 0) {
                        DeallocPhys(frames[k]);
                        --allocated;
                        frames[k] = 0;
                        break;
                    }   
                }
                if ((rng / 77) % 17 == 0) {
                    break;
                }
            }
        }
    }
}

void RegisterTfwPhysTests() {
    // PLEASE NOTE: until we implement virtual memory, it's actually always using the bitmap allocator
    
    RegisterTfwTest("Is AllocPhys sane", TFW_SP_AFTER_PHYS, SanityCheck, PANIC_UNIT_TEST_OK, 0);
    RegisterTfwTest("Basic AllocPhys test (bitmap)", TFW_SP_AFTER_PHYS, BasicAllocationTest, PANIC_UNIT_TEST_OK, 0);
    RegisterTfwTest("Basic AllocPhys test (stack)", TFW_SP_AFTER_HEAP, BasicAllocationTest, PANIC_UNIT_TEST_OK, 0);
    RegisterTfwTest("Basic DeallocPhys test (bitmap)", TFW_SP_AFTER_PHYS, BasicDeallocationTest, PANIC_UNIT_TEST_OK, 0);
    RegisterTfwTest("Basic DeallocPhys test (stack)", TFW_SP_AFTER_HEAP, BasicDeallocationTest, PANIC_UNIT_TEST_OK, 0);

    RegisterTfwTest("AllocPhys and DeallocPhys stress test (bitmap 1)", TFW_SP_AFTER_PHYS, StressTest, PANIC_UNIT_TEST_OK, 1);
    RegisterNightlyTfwTest("AllocPhys and DeallocPhys stress test (bitmap 2)", TFW_SP_AFTER_PHYS, StressTest, PANIC_UNIT_TEST_OK, 2);
    RegisterTfwTest("AllocPhys and DeallocPhys stress test (stack 1)", TFW_SP_AFTER_HEAP, StressTest, PANIC_UNIT_TEST_OK, 0);
    RegisterTfwTest("AllocPhys and DeallocPhys stress test (stack 2)", TFW_SP_AFTER_HEAP, StressTest, PANIC_UNIT_TEST_OK, 1);
    RegisterNightlyTfwTest("AllocPhys and DeallocPhys stress test (stack 3)", TFW_SP_AFTER_HEAP, StressTest, PANIC_UNIT_TEST_OK, 2);
    RegisterNightlyTfwTest("AllocPhys and DeallocPhys stress test (stack 4)", TFW_SP_AFTER_HEAP, StressTest, PANIC_UNIT_TEST_OK, 3);

    RegisterTfwTest("AllocPhys returns page aligned addresses", TFW_SP_AFTER_HEAP, IsPageAligned, PANIC_UNIT_TEST_OK, 0);

    RegisterTfwTest("DeallocPhys only accepts page aligned addresses (1)", TFW_SP_AFTER_HEAP, DeallocationChecksForPageAlignment, PANIC_ASSERTION_FAILURE, 1);
    RegisterTfwTest("DeallocPhys only accepts page aligned addresses (2)", TFW_SP_AFTER_HEAP, DeallocationChecksForPageAlignment, PANIC_ASSERTION_FAILURE, ARCH_PAGE_SIZE / 2);

    RegisterTfwTest("DeallocPhys checks for double allocation", TFW_SP_AFTER_HEAP, DoubleDeallocationFails, PANIC_ASSERTION_FAILURE, 0);
}

#endif
