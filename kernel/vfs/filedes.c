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
    * The only flag that can live here is FD_CLOEXEC. All other flags live on the filesytem
    * level. This is because FD_CLOEXEC is a property of the file descriptor, not the underlying
    * file itself. (This is important in how dup() works.)
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
    table->entries = AllocHeapEx(sizeof(struct filedes_entry) * MAX_FD_PER_PROCESS, HEAP_ALLOW_PAGING);

    for (int i = 0; i < MAX_FD_PER_PROCESS; ++i) {
        table->entries[i].file = NULL;
    }

    return table;
}

struct filedes_table* CopyFileDescriptorTable(struct filedes_table* original) {
    struct filedes_table* new_table = CreateFileDescriptorTable();

    AcquireMutex(original->lock, -1);
    memcpy(new_table->entries, original->entries, sizeof(struct filedes_entry) * MAX_FD_PER_PROCESS);
    ReleaseMutex(original->lock);
    
    return new_table;
}

int CreateFileDescriptor(struct filedes_table* table, struct open_file* file, int* fd_out, int flags) {
    if ((flags & ~FD_CLOEXEC) != 0) {
        return EINVAL;
    }

    AcquireMutex(table->lock, -1);

    for (int i = 0; i < MAX_FD_PER_PROCESS; ++i) {
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

    for (int i = 0; i < MAX_FD_PER_PROCESS; ++i) {
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
    if (out == NULL || fd < 0 || fd >= MAX_FD_PER_PROCESS) {
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

    for (int i = 0; i < MAX_FD_PER_PROCESS; ++i) {
        if (table->entries[i].file != NULL) {
            if (table->entries[i].flags & FD_CLOEXEC) {
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

int DuplicateFileDescriptor(struct filedes_table* table, int oldfd, int* newfd) {
    AcquireMutex(table->lock, -1);

    struct open_file* original_file;
    int res = GetFileFromDescriptor(table, oldfd, &original_file);
    if (res != 0 || original_file == NULL) {
        ReleaseMutex(table->lock);
        return EBADF;
    }

    for (int i = 0; i < MAX_FD_PER_PROCESS; ++i) {
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

int DuplicateFileDescriptor2(struct filedes_table* table, int oldfd, int newfd) {
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
    table->entries[newfd].flags = 0;
    
    ReleaseMutex(table->lock);
    return 0;
}

int DuplicateFileDescriptor3(struct filedes_table* table, int oldfd, int newfd, int flags) {
    /*
    * "If oldfd equals newfd, then dup3() fails with the error EINVAL."
    */
    if (oldfd == newfd) {
        return EINVAL;
    }

    /*
    * Ensure that only valid flags are passed in.
    */
    if ((flags & ~O_CLOEXEC) != 0) {
        return EINVAL;
    }

    int result = DuplicateFileDescriptor2(table, oldfd, newfd);
    if (result != 0) {
        return result;
    }

    AcquireMutex(table->lock, -1);
    if (flags & O_CLOEXEC) {
        table->entries[newfd].flags |= FD_CLOEXEC;
    }
    ReleaseMutex(table->lock);

    return 0;
}