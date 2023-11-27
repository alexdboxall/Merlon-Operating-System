#pragma once

#include <common.h>

enum {
    TFW_SP_INITIAL,
    TFW_SP_AFTER_PHYS,
    TFW_SP_AFTER_HEAP,
    TFW_SP_AFTER_BOOTSTRAP_CPU,
    TFW_SP_AFTER_VIRT,
    TFW_SP_AFTER_PHYS_REINIT,
    TFW_SP_AFTER_ALL_CPU,

    TFW_SP_ALL_CLEAR,
};

#ifdef NDEBUG

#define IsInTfwTest() false
#define FinishedTfwTest(x)
#define MarkTfwStartPoint(x)
#define InitTfw()

#else

bool IsInTfwTest(void);

// this can probably go up to around 150,000 or so in theory (in what the transfer format supports), or about 20,000 on a 4MB RAM system.
// but bigger is slower, so only increase as we need to
#define MAX_TWF_TESTS 100
#define MAX_NAME_LENGTH 96      // If this changes the python must do too


struct tfw_test {
    char name[MAX_NAME_LENGTH];
    void (*code)(struct tfw_test*, size_t context);
    int start_point;
    int expected_panic_code;
    bool nightly_only;
    size_t context;
};

#define TFW_IGNORE_UNUSED (void) test; (void) context;
#define TFW_CREATE_TEST(name) static void name (struct tfw_test* test, size_t context)

void RegisterTfwTest(const char* name, int start_point, void (*code)(struct tfw_test*, size_t), int expected_panic, size_t context);
void RegisterNightlyTfwTest(const char* name, int start_point, void (*code)(struct tfw_test*, size_t), int expected_panic, size_t context);

void FinishedTfwTest(int panic_code);
void MarkTfwStartPoint(int id);
void InitTfw(void);

#endif

