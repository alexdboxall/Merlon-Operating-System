
#include <fs/internal/fat.h>
#include <errno.h>

int DetectFatPartition(void* partition) {
    (void) partition;
    return ENOTSUP;
}