#include <filedes.h>
#include <errno.h>
#include <string.h>
#include <common.h>
#include <semaphore.h>
#include <vfs.h>
#include <fcntl.h>
#include <log.h>
#include <heap.h>
#include <irql.h>

struct filedes_entry {
    /*
     * Set to NULL if this entry isn't in use.
     */
    struct open_file* file;

    /*
     * The only flag here is FD_CLOEXEC (== O_CLOEXEC for this OS).
     */
    int flags;
};

/*
* The table of all of the file descriptors in use by a process.
*/
struct filedes_table {
    struct semaphore* lock;
    struct filedes_entry* entries;
};

struct filedes_table* CreateFileDescriptorTable(void) {
    struct filedes_table* table = AllocHeap(sizeof(struct filedes_table));

    table->lock = CreateMutex("filedes");
    table->entries = AllocHeapEx(
        sizeof(struct filedes_entry) * PROC_MAX_FD, HEAP_ALLOW_PAGING
    );

    for (int i = 0; i < PROC_MAX_FD; ++i) {
        table->entries[i].file = NULL;
    }

    return table;
}

struct filedes_table* CopyFileDescriptorTable(struct filedes_table* original) {
    struct filedes_table* new_table = CreateFileDescriptorTable();

    AcquireMutex(original->lock, -1);
    memcpy(new_table->entries, original->entries, sizeof(struct filedes_entry) * PROC_MAX_FD);
    ReleaseMutex(original->lock);
    
    return new_table;
}

void DestroyFileDescriptorTable(struct filedes_table* table) {
    AcquireMutex(table->lock, -1);

    for (int i = 0; i < PROC_MAX_FD; ++i) {
        if (table->entries[i].file != NULL) {
            CloseFile(table->entries[i].file);
        }
    }

    ReleaseMutex(table->lock);
    DestroyMutex(table->lock);
}

int CreateFileDescriptor(
    struct filedes_table* table, struct open_file* file, int* fd_out, int flags
) {
    if ((flags & ~O_CLOEXEC) != 0) {
        return EINVAL;
    }

    AcquireMutex(table->lock, -1);

    for (int i = 0; i < PROC_MAX_FD; ++i) {
        if (table->entries[i].file == NULL) {
            table->entries[i].file = file;
            table->entries[i].flags = flags;
            ReleaseMutex(table->lock);
            *fd_out = i;
            return 0;
        }
    }

    ReleaseMutex(table->lock);
    return EMFILE;
}

int RemoveFileDescriptor(struct filedes_table* table, struct open_file* file) {
    AcquireMutex(table->lock, -1);

    for (int i = 0; i < PROC_MAX_FD; ++i) {
        if (table->entries[i].file == file) {
            table->entries[i].file = NULL;
            ReleaseMutex(table->lock);
            return 0;
        }
    }

    ReleaseMutex(table->lock);
    return EINVAL;
}

int GetFileFromDescriptor(struct filedes_table* table, int fd, struct open_file** out) {
    if (out == NULL || fd < 0 || fd >= PROC_MAX_FD) {
        *out = NULL;
        return out == NULL ? EINVAL : EBADF;
    }

    AcquireMutex(table->lock, -1);
    struct open_file* result = table->entries[fd].file;
    ReleaseMutex(table->lock);

    *out = result;
    return result == NULL ? EBADF : 0;
}

int HandleFileDescriptorsOnExec(struct filedes_table* table) {
    AcquireMutex(table->lock, -1);

    for (int i = 0; i < PROC_MAX_FD; ++i) {
        if (table->entries[i].file != NULL) {
            if (table->entries[i].flags & O_CLOEXEC) {
                struct open_file* file = table->entries[i].file;
                table->entries[i].file = NULL;
                int res = CloseFile(file);
                if (res != 0) {
                    ReleaseMutex(table->lock);
                    return res;
                }
            }
        }
    }

    ReleaseMutex(table->lock);
    return 0;
}

int DupFd(struct filedes_table* table, int oldfd, int* newfd) {
    AcquireMutex(table->lock, -1);

    struct open_file* original_file;
    int res = GetFileFromDescriptor(table, oldfd, &original_file);
    if (res != 0 || original_file == NULL) {
        ReleaseMutex(table->lock);
        return EBADF;
    }

    for (int i = 0; i < PROC_MAX_FD; ++i) {
        if (table->entries[i].file == NULL) {
            table->entries[i].file = original_file;
            table->entries[i].flags = 0;
            ReleaseMutex(table->lock);
            *newfd = i;
            return 0;
        }
    }

    ReleaseMutex(table->lock);
    return EMFILE;
}

int DupFd2(struct filedes_table* table, int oldfd, int newfd, int flags) {
    if (flags & ~O_CLOEXEC) {
        return EINVAL;
    }

    AcquireMutex(table->lock, -1);

    struct open_file* original_file;
    int res = GetFileFromDescriptor(table, oldfd, &original_file);

    /*
    * "If oldfd is not a valid file descriptor, then the call fails,
    * and newfd is not closed."
    */
    if (res != 0 || original_file == NULL) {
        ReleaseMutex(table->lock);
        return EBADF;
    }

    /*
    * "If oldfd is a valid file descriptor, and newfd has the same
    * value as oldfd, then dup2() does nothing..."
    */
    if (oldfd == newfd) {
        ReleaseMutex(table->lock);
        return 0;
    }

    struct open_file* current_file;
    res = GetFileFromDescriptor(table, oldfd, &current_file);
    if (res == 0 && current_file != NULL) {
        /*
        * "If the file descriptor newfd was previously open, it is closed
        * before being reused; the close is performed silently (i.e., any
        * errors during the close are not reported by dup2())."
        */
        CloseFile(current_file);
    }

    table->entries[newfd].file = original_file;
    table->entries[newfd].flags = flags;
    
    ReleaseMutex(table->lock);
    return 0;
}
