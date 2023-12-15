
#include <transfer.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <virtual.h>
#include <arch.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))

static int ValidateCopy(const void* user_addr, size_t size, bool write) {
    size_t initial_address = (size_t) user_addr;

    /*
    * Check if the memory range starts in user memory.
    */
    if (initial_address < ARCH_USER_AREA_BASE || initial_address >= ARCH_USER_AREA_LIMIT) {
        return EINVAL;
    }

    size_t final_address = initial_address + size;

    /*
    * Check for overflow when the initial address and size are added. If it would overflow,
    * we cancel the operation, as the user is obviously outside their range.
    */
    if (final_address < initial_address) {
        return EINVAL;
    }

    /*
    * Ensure the end of the memory range is in user memory. As user memory must be contiguous,
    * this ensures the entire range is in user memory.
    */
    if (final_address < ARCH_USER_AREA_BASE || final_address >= ARCH_USER_AREA_LIMIT) {
        return EINVAL;
    }

    /*
    * We must now check if the USER (and possibly WRITE) bits are set on the memory pages
    * being accessed.
    */
    size_t initial_page = initial_address / ARCH_PAGE_SIZE;
    size_t pages = (size + ARCH_PAGE_SIZE - 1) / ARCH_PAGE_SIZE;

    for (size_t i = 0; i < pages; ++i) {
        size_t page = initial_page + i;
        size_t permissions = GetVirtPermissions(page * ARCH_PAGE_SIZE);

        if (permissions == 0) {
            return EINVAL;
        }

        if (!(permissions & VM_READ)) {
            return EINVAL;
        }
        if (!(permissions & VM_USER)) {
            return EINVAL;
        }
        if (write && !(permissions & VM_WRITE)) {
            return EINVAL;
        }
        if (write && (permissions & VM_EXEC)) {
            return EINVAL;
        }
        if (write && (permissions & VM_EXEC)) {
            return EINVAL;
        }
    }
    
    return 0;
}

static int CopyIntoKernel(void* kernel_addr, const void* user_addr, size_t size) {
    int status = ValidateCopy(user_addr, size, false);
    if (status != 0) {
        return status;
    }

    inline_memcpy(kernel_addr, user_addr, size);
    return 0;
}

static int CopyOutOfKernel(const void* kernel_addr, void* user_addr, size_t size) {
    int status = ValidateCopy(user_addr, size, true);
    if (status != 0) {
        return status;
    }

    inline_memcpy(user_addr, kernel_addr, size);
    return 0;
}

int PerformTransfer(void* trusted_buffer, struct transfer* untrusted_buffer, uint64_t len) { 
    assert(trusted_buffer != NULL);
    assert(untrusted_buffer != NULL && untrusted_buffer->address != NULL);
    assert(untrusted_buffer->direction == TRANSFER_READ || untrusted_buffer->direction == TRANSFER_WRITE);

    size_t amount_to_copy = MIN(len, untrusted_buffer->length_remaining);
    if (amount_to_copy == 0) {
        return 0;
    }

    if (untrusted_buffer->type == TRANSFER_INTRA_KERNEL) {
        if (untrusted_buffer->direction == TRANSFER_READ) {
            memmove(untrusted_buffer->address, trusted_buffer, amount_to_copy);
        
        } else {
            memmove(trusted_buffer, untrusted_buffer->address, amount_to_copy);
        } 

    } else {
        int result;

        /*
        * This is from the kernel's perspective of the operations.
        */
        if (untrusted_buffer->direction == TRANSFER_READ) {
            result = CopyOutOfKernel((const void*) trusted_buffer, untrusted_buffer->address, amount_to_copy);
            
        } else {
            result = CopyIntoKernel(trusted_buffer, (const void*) untrusted_buffer->address, amount_to_copy);

        }

        if (result != 0) {
            return result;
        }
    }

    untrusted_buffer->length_remaining -= amount_to_copy;
    untrusted_buffer->offset += amount_to_copy;
    untrusted_buffer->address = ((uint8_t*) untrusted_buffer->address) + amount_to_copy;

    return 0;
}  

int WriteStringToUsermode(char* trusted_string, char* untrusted_buffer, uint64_t max_length) {
    struct transfer tr = CreateTransferWritingToUser(untrusted_buffer, max_length, 0);
    int result;

    /*
     * Limit the size of the string by the maximimum length. We use <, and a -1 in the other case,
     * as we need to ensure the null terminator fits.
     */
    uint64_t size = strlen(trusted_string) < max_length ? strlen(trusted_string) : max_length - 1;
    result = PerformTransfer(trusted_string, &tr, size);

    if (result != 0) {
        return result;
    }
    
    uint8_t zero = 0;
    return PerformTransfer(&zero, &tr, 1);
}

int ReadStringFromUsermode(char* trusted_buffer, char* untrusted_string, uint64_t max_length) {
    struct transfer tr = CreateTransferReadingFromUser(untrusted_string, max_length, 0);
    size_t i = 0;

    while (max_length--) {
        char c;
        int result = PerformTransfer(&c, &tr, 1);
        if (result != 0) {
            return result;
        }
        trusted_buffer[i++] = c;
        if (c == 0) {
            break;
        }
    }
    
    return 0;
}

static struct transfer CreateTransfer(void* addr, uint64_t length, uint64_t offset, int direction, int type) {
    struct transfer trans;
    trans.address = addr;
    trans.direction = direction;
    trans.length_remaining = length;
    trans.offset = offset;
    trans.type = type;
    return trans;
}

struct transfer CreateKernelTransfer(void* addr, uint64_t length, uint64_t offset, int direction) {
    return CreateTransfer(addr, length, offset, direction, TRANSFER_INTRA_KERNEL);
}

struct transfer CreateTransferWritingToUser(void* untrusted_addr, uint64_t length, uint64_t offset) {
    /*
     * When we "write to the user", we are doing so because the user is trying to *read* from the kernel.
     * i.e. someone is doing an "untrusted read" of kernel data (i.e. a TRANSFER_READ).
     */
    return CreateTransfer(untrusted_addr, length, offset, TRANSFER_READ, TRANSFER_USERMODE);
}

struct transfer CreateTransferReadingFromUser(void* untrusted_addr, uint64_t length, uint64_t offset) {
    /*
     * When we "read from the user", we are doing so because the user is trying to *write* to the kernel.
     * i.e. as they are writing to the kernel, it is an "untrusted write" (i.e. a TRANSFER_WRITE).
     */
    return CreateTransfer(untrusted_addr, length, offset, TRANSFER_WRITE, TRANSFER_USERMODE);
}
