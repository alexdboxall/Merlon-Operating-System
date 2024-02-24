
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>

struct DIR {
    int fd;
};

DIR* opendir(const char* name) {
    if (name == NULL || name[0] == 0) {
        errno = ENOENT;
        return NULL;
    }

    int fd = open(name, O_RDONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
    if (fd == -1) {
        /* errno is already set */
        return NULL;
    }

    struct stat st;
    int res = fstat(fd, &st);
    if (res == -1) {
        return NULL;
    }
    if (!S_ISDIR(st.st_mode)) {
        errno = ENOTDIR;
        return NULL;
    }

    DIR* dir = malloc(sizeof(struct DIR));
    if (dir == NULL) {
        errno = ENOMEM;
        return NULL;
    }
    dir->fd = fd;
    return dir;
}

struct dirent* readdir(DIR* dir) {
    /*
     * This is allowed by the standard.
     */
    static struct dirent ent;

    if (dir == NULL) {
        errno = EBADF;
        return NULL;
    }

    int old_errno = errno;

retry:
    errno = 0;
    ssize_t br = read(dir->fd, &ent, sizeof(struct dirent));
    if (errno == EINTR) {
        goto retry;
    }
    if (errno == EBADF || errno == ENOSYS) {
        return NULL;
    }

    errno = old_errno;
    if (errno == 0 && br == sizeof(struct dirent)) {
        return &ent;
    } else {
        /* 
         * For other errors / incomplete reads, we will keep the old errno (i.e. 
         * indicate there is no error here) - this means we are at EOF.
         */
        return NULL;
    }
}

int closedir(DIR* dir) {
    if (dir == NULL) {
        return EBADF;
    }

    int fd = dir->fd;
    free(dir);
    return close(fd);
}