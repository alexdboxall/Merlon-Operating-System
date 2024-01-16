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

static struct file* swapfile = NULL;
static struct spinlock swapfile_lock;
static uint8_t* swapfile_bitmap;
static int number_on_swapfile = 0;
static size_t bits_in_bitmap = 0;

static int GetPageCountForBitmap(void) {
    /*
     * How many bytes of swapfile that each page in the bitmap keeps track of. 
     */
    uint64_t accessible_per_page = 8 * ARCH_PAGE_SIZE * ARCH_PAGE_SIZE;
    size_t target_size = (GetTotalPhysKilobytes() * 4 + 32 * 1024) * 1024;
    return (target_size + accessible_per_page - 1) / accessible_per_page;
}

static void SetupSwapfileBitmap(void) {
    int pages_in_bitmap = GetPageCountForBitmap();
    bits_in_bitmap = 8 * pages_in_bitmap * ARCH_PAGE_SIZE;

    swapfile_bitmap = (uint8_t*) MapVirt(
        0, 0, pages_in_bitmap * ARCH_PAGE_SIZE, 
        VM_READ | VM_WRITE | VM_LOCK, NULL, 0
    );
}

void InitSwapfile(void) {
    if (OpenFile("swap:/", O_RDWR, 0, &swapfile)) {
        Panic(PANIC_NO_FILESYSTEM);
    }
    
    InitSpinlock(&swapfile_lock, "swapfile", IRQL_SCHEDULER);
    SetupSwapfileBitmap();
}

struct file* GetSwapfile(void) {
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

uint64_t AllocSwap(void) {
    AcquireSpinlock(&swapfile_lock);
    ++number_on_swapfile;

    for (size_t i = 0; i < bits_in_bitmap; ++i) {
        if (!GetBitmapEntry(i)) {
            SetBitmapEntry(i, true);
            ReleaseSpinlock(&swapfile_lock);
            return i;
        }
    }

    Panic(PANIC_OUT_OF_SWAPFILE);
}

void DeallocSwap(uint64_t index) {
    AcquireSpinlock(&swapfile_lock);
    --number_on_swapfile;
    SetBitmapEntry(index, false);
    ReleaseSpinlock(&swapfile_lock);
}

int GetSwapCount(void) {
    AcquireSpinlock(&swapfile_lock);
    int val = number_on_swapfile;
    ReleaseSpinlock(&swapfile_lock);
    return val;
}