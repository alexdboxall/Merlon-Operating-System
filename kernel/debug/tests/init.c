
#include <debug.h>
#include <assert.h>
#include <panic.h>
#include <string.h>
#include <log.h>
#include <arch.h>
#include <physical.h>

#ifndef NDEBUG

TFW_CREATE_TEST(BootSuccessful) { TFW_IGNORE_UNUSED
    
}

void RegisterTfwInitTests(void) {
    RegisterTfwTest("Is boot successful", TFW_SP_ALL_CLEAR, BootSuccessful, PANIC_UNIT_TEST_OK, 0);
}

#endif
void RegisterTfwAVLTreeTests(void);
void RegisterTfwPriorityQueueTests(void);
