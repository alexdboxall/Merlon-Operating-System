#include <os/time.h>

uint64_t OsGetUtcTime(void) {
    uint64_t local_time = OsGetLocalTime();
    uint64_t offset;
    int res = OsGetTimezone(NULL, 0, &offset);
    if (res != 0) {
        return 0;
    }
    return local_time - offset;
}
