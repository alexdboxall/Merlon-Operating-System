#pragma once

#include <stddef.h>
#include <sys/types.h>
#include <stdbool.h>
#include <common.h>
#include <arch.h>

#define VM_READ         1
#define VM_WRITE        2
#define VM_USER         4
#define VM_EXEC         8
#define VM_LOCK         16
#define VM_FILE         32
#define VM_FIXED_VIRT   64
#define VM_MAP_HARDWARE 128     /* map a physical page that doesn't live within the physical memoery manager*/
#define VM_LOCAL        256     /* indicates it's local to the VAS - i.e. not in kernel global memory */
#define VM_RECURSIVE    512     /* assumes the VAS is already locked, so won't lock or unlock it */
#define VM_RELOCATABLE  1024    /* needs driver fixups whenever swapped back in*/
#define VM_EVICT_FIRST  2048
#define VM_SHARED       4096    /* fork() will cause the memory to be shared */
#define VM_HARD_IO_FAIL 8192    /* see `hard_io_failure` */

#define VMUN_ALLOW_NON_EXIST    1

#define VAS_NO_ARCH_INIT    1

struct file;

struct vas_entry {
    size_t virtual;
    
    uint8_t in_ram          : 1;        /* Whether it is backed by a physical page or not. (i.e. does it have a real page table entry) */ 
    uint8_t allocated       : 1;        /* Whether or not to free a physical page on deallocation. Differs from in_ram when VM_MAP_HARDWARE is set. */
    uint8_t file            : 1;        /* Whether or not the page is file-mapped. */
    uint8_t cow             : 1;        /* */
    uint8_t swapfile        : 1;        /* Whether or not the page has been moved to a swapfile. Will not occur if 'file' is set (will back to that file instead)*/
    uint8_t                 : 1;           
    uint8_t read            : 1;
    uint8_t write           : 1;

    uint8_t exec            : 1;
    uint8_t user            : 1;
    uint8_t global          : 1;
    uint8_t allow_temp_write: 1;        /* used internally - allows the system to write to otherwise read-only pages to, e.g. reload from disk */
    uint8_t relocatable     : 1;        /* from a relocated driver file             */
    uint8_t first_load      : 1;
    uint8_t load_in_progress: 1;        /* someone else is deferring a read into this page - keep trying the access until flag clears */
    
    uint8_t times_swapped   : 4;
    uint8_t evict_first     : 1;
    uint8_t share_on_fork   : 1;
    uint8_t hard_io_failure : 1;        /* if set, kernel mode, file mapped I/O failures will panic, if clear it will return a zero page */
    uint8_t                 : 1;

    int lock;
    int num_pages;                      /* only used for non-allocated or hardware mapped to reduce the number of AVL entries */

    off_t file_offset;
    struct file* file_node;
    size_t physical;
    union {
        size_t swapfile_offset;
        size_t relocation_base;
    };

    int ref_count;
};

struct vas;

size_t BytesToPages(size_t bytes);

bool LockVirt(size_t virtual);
void UnlockVirt(size_t virtual);
bool LockVirtEx(struct vas* vas, size_t virtual);
void UnlockVirtEx(struct vas* vas, size_t virtual);

int SetVirtPermissionsEx(struct vas* vas, size_t virtual, int set, int clear);
int SetVirtPermissions(size_t virtual, int set, int clear);
int GetVirtPermissions(size_t virtual);

size_t MapVirt(size_t physical, size_t virtual, size_t bytes, int flags, struct file* file, off_t pos);
size_t MapVirtEx(struct vas* vas, size_t physical, size_t virtual, size_t pages, int flags, struct file* file, off_t pos, int* error);

#define MapVirtEasy(bytes, pageable) ((void*) MapVirt(0, 0, bytes, VM_READ | VM_WRITE | (pageable ? 0 : VM_LOCK), NULL, 0))

int UnmapVirt(size_t virtual, size_t bytes);
int UnmapVirtEx(struct vas* vas, size_t virtual, size_t pages, int flags);
int WipeUsermodePages(void);

size_t GetPhysFromVirt(size_t virtual);

struct vas* GetKernelVas(void);     // a kernel vas
struct vas* GetVas(void);           // current vas

struct vas* CreateVas(void);
void CreateVasEx(struct vas* vas, int flags);
void DestroyVas(struct vas* vas);

struct vas* CopyVas(struct vas* new_vas);
void SetVas(struct vas* vas);
void InitVirt(void);
bool IsVirtInitialised(void);
void EvictVirt(void);
void HandleVirtFault(size_t faulting_virt, int fault_type);

#include <arch.h>
#include <spinlock.h>

struct vas {
    struct tree* mappings;
    platform_vas_data_t* arch_data;
    struct spinlock lock;
};
