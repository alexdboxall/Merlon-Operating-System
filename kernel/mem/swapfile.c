#include <virtual.h>
#include <vfs.h>
#include <fcntl.h>
#include <log.h>
#include <errno.h>
#include <panic.h>
#include <transfer.h>
#include <arch.h>

struct open_file* swapfile = NULL;

void InitSwapfile(void) {
    int res = OpenFile("swap:/", O_RDWR, 0, &swapfile);
    if (res != 0) {
        LogDeveloperWarning("No swapfile found!!\n");
        swapfile = NULL;
    }
}

struct open_file* GetSwapfile(void) {
    return swapfile;
}

// TODO: lock
uint64_t AllocateSwapfileIndex(void) {
    static uint64_t next = 0;
    return next++;
}

void DeallocateSwapfileIndex(uint64_t index) {
    (void) index;
}