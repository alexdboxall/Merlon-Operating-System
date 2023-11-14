#pragma once

#include <common.h>


#ifdef NDEBUG

#define IsInTfwTest() false
#define FinishedTfwTest(x)
#define MarkTfwStartPoint(x)
#define InitTfw()

#else

bool IsInTfwTest(void);

#define MAX_TWF_TESTS 100
#define MAX_NAME_LENGTH 96

enum {
    TFW_SP_INITIAL,
    TFW_SP_AFTER_PHYS,
    TFW_SP_AFTER_HEAP,
    TFW_SP_AFTER_BOOTSTRAP_CPU,
    TFW_SP_AFTER_VIRT,
    TFW_SP_AFTER_HEAP_REINIT,
    TFW_SP_AFTER_ALL_CPU,
};

struct tfw_test {
    char name[MAX_NAME_LENGTH];
    void (*code)(struct tfw_test*);
    int start_point;
    int expected_panic_code;
};

void RegisterTfwTest(const char* name, int start_point, void (*code)(struct tfw_test*), int expected_panic);
void FinishedTfwTest(int panic_code);
void MarkTfwStartPoint(int id);
void InitTfw(void);

#endif

