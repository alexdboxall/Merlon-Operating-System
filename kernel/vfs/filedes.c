#include <filedes.h>
#include <errno.h>
#include <string.h>
#include <common.h>
#include <semaphore.h>
#include <vfs.h>
#include <fcntl.h>
#include <log.h>
#include <heap.h>
#include <virtual.h>
#include <irql.h>

struct fd {
    struct file* file;      /* NULL if entry isn't in use */
    int flags;              /* FD_CLOEXEC ( == O_CLOEXEC for this OS)*/
};

struct fd_table {
    struct semaphore* lock;
    struct fd* entries;
};

#define TABLE_SIZE (sizeof(struct fd) * PROC_MAX_FD)

struct fd_table* CreateFdTable(void) {
    EXACT_IRQL(IRQL_STANDARD);

    struct fd_table* table = AllocHeap(sizeof(struct fd_table));

    table->lock = CreateMutex("filedes");
    table->entries = MapVirtEasy(TABLE_SIZE, true);

    for (int i = 0; i < PROC_MAX_FD; ++i) {
        table->entries[i].file = NULL;
    }

    return table;
}

struct fd_table* CopyFdTable(struct fd_table* original) {
    EXACT_IRQL(IRQL_STANDARD);

    struct fd_table* new_table = CreateFdTable();

    AcquireMutex(original->lock, -1);
    memcpy(new_table->entries, original->entries, TABLE_SIZE);
    ReleaseMutex(original->lock);
    
    return new_table;
}

void DestroyFdTable(struct fd_table* table) {
    EXACT_IRQL(IRQL_STANDARD);

    AcquireMutex(table->lock, -1);

    for (int i = 0; i < PROC_MAX_FD; ++i) {
        if (table->entries[i].file != NULL) {
            CloseFile(table->entries[i].file);
        }
    }

    UnmapVirt((size_t) table->entries, TABLE_SIZE);

    ReleaseMutex(table->lock);
    DestroyMutex(table->lock);
}

int CreateFd(struct fd_table* table, struct file* file, int* fd_out, int flags) {
    EXACT_IRQL(IRQL_STANDARD);

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
            LogWriteSerial("Creating fd... file = [0x%X, 0x%X], fd = %d\n", file, file->node, i);
            return 0;
        }
    }

    ReleaseMutex(table->lock);
    return EMFILE;
}

int RemoveFd(struct fd_table* table, struct file* file) {
    EXACT_IRQL(IRQL_STANDARD);

    AcquireMutex(table->lock, -1);

    for (int i = 0; i < PROC_MAX_FD; ++i) {
        if (table->entries[i].file == file) {
            table->entries[i].file = NULL;
            LogWriteSerial("Removing fd... file = [0x%X, 0x%X], fd = %d\n", file, file->node, i);
            ReleaseMutex(table->lock);
            return 0;
        }
    }

    ReleaseMutex(table->lock);
    return EINVAL;
}

int GetFileFromFd(struct fd_table* table, int fd, struct file** out) {
    EXACT_IRQL(IRQL_STANDARD);

    if (out == NULL || fd < 0 || fd >= PROC_MAX_FD) {
        *out = NULL;
        return out == NULL ? EINVAL : EBADF;
    }

    AcquireMutex(table->lock, -1);
    struct file* result = table->entries[fd].file;
    ReleaseMutex(table->lock);

    *out = result;
    return result == NULL ? EBADF : 0;
}

int HandleExecFd(struct fd_table* table) {
    EXACT_IRQL(IRQL_STANDARD);

    AcquireMutex(table->lock, -1);

    for (int i = 0; i < PROC_MAX_FD; ++i) {
        if (table->entries[i].file != NULL) {
            if (table->entries[i].flags & O_CLOEXEC) {
                struct file* file = table->entries[i].file;
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

int DupFd(struct fd_table* table, int oldfd, int* newfd) {
    EXACT_IRQL(IRQL_STANDARD);

    AcquireMutex(table->lock, -1);

    struct file* original_file;
    int res = GetFileFromFd(table, oldfd, &original_file);
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

int DupFd2(struct fd_table* table, int oldfd, int newfd, int flags) {
    EXACT_IRQL(IRQL_STANDARD);

    if (flags & ~O_CLOEXEC) {
        return EINVAL;
    }

    AcquireMutex(table->lock, -1);

    struct file* original_file;
    int res = GetFileFromFd(table, oldfd, &original_file);

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

    struct file* current_file;
    res = GetFileFromFd(table, oldfd, &current_file);
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
