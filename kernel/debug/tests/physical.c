
#include <debug.h>
#include <assert.h>
#include <panic.h>
#include <string.h>
#include <log.h>
#include <arch.h>
#include <physical.h>

#ifndef NDEBUG

static void IsPageAligned(struct tfw_test* test) {
    (void) test;
    assert(AllocPhys() % ARCH_PAGE_SIZE == 0);
}

static void DeallocationChecksForPageAlignment1(struct tfw_test* test) {
    (void) test;
    size_t p = AllocPhys();
    DeallocPhys(p + 1);
}

static void DeallocationChecksForPageAlignment2(struct tfw_test* test) {
    (void) test;
    size_t p = AllocPhys();
    DeallocPhys(p + ARCH_PAGE_SIZE / 2);
}

static void DoubleDeallocationFails(struct tfw_test* test) {
    (void) test;
    size_t p = AllocPhys();
    DeallocPhys(p);
    DeallocPhys(p);
}

static void StressTest(struct tfw_test* test) {
    (void) test;

    size_t frames[512];
    memset(frames, 0, sizeof(frames));
    int allocated = 0;

    uint32_t next = 12;
    
    for (int i = 0; i < 100000; ++i) {
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
    RegisterTfwTest("AllocPhys returns page aligned addresses", TFW_SP_AFTER_HEAP, IsPageAligned, PANIC_UNIT_TEST_OK);
    RegisterTfwTest("DeallocPhys only accepts page aligned addresses (1)", TFW_SP_AFTER_HEAP, DeallocationChecksForPageAlignment1, PANIC_ASSERTION_FAILURE);
    RegisterTfwTest("DeallocPhys only accepts page aligned addresses (2)", TFW_SP_AFTER_HEAP, DeallocationChecksForPageAlignment2, PANIC_ASSERTION_FAILURE);
    RegisterTfwTest("DeallocPhys checks for double allocation", TFW_SP_AFTER_HEAP, DoubleDeallocationFails, PANIC_ASSERTION_FAILURE);
    RegisterTfwTest("AllocPhys and DeallocPhys stress test", TFW_SP_AFTER_HEAP, StressTest, PANIC_UNIT_TEST_OK);
}

#endif
