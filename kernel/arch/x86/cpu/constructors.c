
#include <log.h>

typedef void initfunc_t(void);
extern initfunc_t* start_ctors[];
extern initfunc_t* end_ctors[];

__attribute__ ((constructor)) void foo(void) {
	LogWriteSerial("Global constructors work.\n");
}
 
void ArchCallGlobalConstructors() {
    for (initfunc_t** p = start_ctors; p != end_ctors; p++) {
        LogWriteSerial("found ctor at 0x%X\n", p);
        LogWriteSerial("deref gives 0x%X\n", *p);
        (*p)();
    }
    LogWriteSerial("All global constructors have run.\n");
}
