#include "krnlapi.h"
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

unsigned int alarm(unsigned int secs) {
    uint64_t us_in = ((uint64_t) secs) * 1000000ULL;
    uint64_t us_out;
    _system_call(SYSCALL_ALARM, (size_t) &us_in, (size_t) &us_out, 0, 0, 0);
    
    return (us_out + 999999) / 1000000;
}