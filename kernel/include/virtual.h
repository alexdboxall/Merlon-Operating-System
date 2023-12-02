#pragma once

#include <stddef.h>
#include <sys/types.h>
#include <stdbool.h>
#include <common.h>

#define VM_READ         1
#define VM_WRITE        2
#define VM_USER         4
#define VM_EXEC         8
#define VM_LOCK         16
#define VM_FILE         32
#define VM_FIXED_VIRT   64
#define VM_MAP_HARDWARE 128     /* map a physical page that doesn't live within the physical memoery manager*/
#define VM_LOCAL        256     /* indicates it's local to the VAS - i.e. not in kernel global memory */

#define VAS_NO_ARCH_INIT    1

struct vas_entry {
    size_t virtual;
    
    uint8_t in_ram          : 1;        /* Whether it is backed by a physical page or not. (i.e. does it have a real page table entry) */ 
    uint8_t allocated       : 1;        /* Whether or not to free a physical page on deallocation. Differs from in_ram when VM_MAP_HARDWARE is set. */
    uint8_t file            : 1;        /* Whether or not the page is file-mapped. */
    uint8_t cow             : 1;        /* */
    uint8_t swapfile        : 1;        /* Whether or not the page has been moved to a swapfile. Will not occur if 'file' is set (will back to that file instead)*/
    uint8_t lock            : 1;           
    uint8_t read            : 1;
    uint8_t write           : 1;

    uint8_t exec            : 1;
    uint8_t user            : 1;
    uint8_t global          : 1;
    uint8_t                 : 3;

    off_t file_offset;
    void* file_node;
    size_t physical;

    int ref_count;
};

struct vas;

size_t BytesToPages(size_t bytes);

void LockVirt(size_t virtual);
void UnlockVirt(size_t virtual);
void SetVirtPermissions(size_t virtual, int set, int clear);
int GetVirtPermissions(size_t virtual);
size_t MapVirt(size_t physical, size_t virtual, size_t bytes, int flags, void* file, off_t pos);
void UnmapVirt(size_t virtual, size_t bytes);
size_t GetPhysFromVirt(size_t virtual);

struct vas* GetKernelVas(void);     // a kernel vas
struct vas* GetVas(void);           // current vas

struct vas* CreateVas(void);
void CreateVasEx(struct vas* vas, int flags);
void DestroyVas(struct vas* vas);

struct vas* CopyVas(void);
void SetVas(struct vas* vas);
void InitVirt(void);
bool IsVirtInitialised(void);
void EvictVirt(void);
void HandleVirtFault(size_t faulting_virt, int fault_type);

#include <arch.h>
#include <spinlock.h>

struct vas {
    struct avl_tree* mappings;
    platform_vas_data_t* arch_data;
    struct spinlock lock;
};
