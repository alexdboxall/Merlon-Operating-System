#include <virtual.h>
#include <vfs.h>
#include <fcntl.h>
#include <log.h>
#include <errno.h>
#include <panic.h>
#include <transfer.h>
#include <irql.h>
#include <virtual.h>
#include <physical.h>
#include <spinlock.h>
#include <arch.h>

static struct open_file* swapfile = NULL;
static struct spinlock swapfile_lock;
static uint8_t* swapfile_bitmap;
static int number_on_swapfile = 0;
static size_t num_swapfile_bitmap_entries = 0;

static int GetPagesRequiredForAllocationBitmap(void) {
    uint64_t bits_per_page = ARCH_PAGE_SIZE * 8;
    uint64_t accessable_per_page = ARCH_PAGE_SIZE * bits_per_page;
    size_t max_swapfile_size = (GetTotalPhysKilobytes() * 1024) * 4 + (32 * 1024 * 1024);
    return (max_swapfile_size + accessable_per_page - 1) / accessable_per_page;
}

static void SetupSwapfileBitmap() {
    int num_pages_in_bitmap = GetPagesRequiredForAllocationBitmap();
    num_swapfile_bitmap_entries = 8 * num_pages_in_bitmap * ARCH_PAGE_SIZE;
    swapfile_bitmap = (uint8_t*) MapVirt(0, 0, num_pages_in_bitmap * ARCH_PAGE_SIZE, VM_READ | VM_WRITE | VM_LOCK, NULL, 0);
}

void InitSwapfile(void) {
    int res = OpenFile("swap:/", O_RDWR, 0, &swapfile);
    if (res != 0) {
        Panic(PANIC_NO_FILESYSTEM);
    }
    
    InitSpinlock(&swapfile_lock, "swapfile", IRQL_SCHEDULER);
    SetupSwapfileBitmap();
}

struct open_file* GetSwapfile(void) {
    return swapfile;
}

static bool GetBitmapEntry(size_t index) {
    return swapfile_bitmap[index / 8] & (1 << (index % 8));
}

static void SetBitmapEntry(size_t index, bool value) {
    if (value) {
        swapfile_bitmap[index / 8] |= 1 << (index % 8);
    } else {
        swapfile_bitmap[index / 8] &= ~(1 << (index % 8));
    }
}

uint64_t AllocateSwapfileIndex(void) {
    AcquireSpinlockIrql(&swapfile_lock);
    ++number_on_swapfile;

    for (size_t i = 0; i < num_swapfile_bitmap_entries; ++i) {
        if (!GetBitmapEntry(i)) {
            SetBitmapEntry(i, true);
            ReleaseSpinlockIrql(&swapfile_lock);
            return i;
        }
    }

    Panic(PANIC_OUT_OF_SWAPFILE);
}

void DeallocateSwapfileIndex(uint64_t index) {
    AcquireSpinlockIrql(&swapfile_lock);
    --number_on_swapfile;
    SetBitmapEntry(index, false);
    ReleaseSpinlockIrql(&swapfile_lock);
}

int GetNumberOfPagesOnSwapfile(void) {
    AcquireSpinlockIrql(&swapfile_lock);
    int val = number_on_swapfile;
    ReleaseSpinlockIrql(&swapfile_lock);
    return val;
}