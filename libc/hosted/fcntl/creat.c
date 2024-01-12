#include <fcntl.h>

int creat(const char* filename, mode_t mode) {
    return open(filename, O_WRONLY | O_CREAT | O_TRUNC, mode);
}
