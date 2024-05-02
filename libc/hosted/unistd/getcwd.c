#include <unistd.h>
#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <errno.h>
#include <dirent.h>

static char* GetName(const char* path, ino_t target) {
    DIR* dir = opendir(path);
    if (dir == NULL) {
        return 0;
    }
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_ino == target) {
            char* res = strdup(entry->d_name);
            closedir(dir);
            return res;
        }
    }
    closedir(dir);
    return 0;
}

static ino_t GetIno(const char* path) {
    DIR* dir = opendir(path);
    if (dir == NULL) {
        return 0;
    }
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (!strcmp(entry->d_name, ".")) {
            closedir(dir);
            return entry->d_ino;
        }
    }
    closedir(dir);
    return 0;
}

static bool AddDeviceRoot(char* buf, size_t* buf_ptr, ino_t current_ino) {
    DIR* dir = opendir("*:/");
    if (dir == NULL) {
        return false;
    }
    int res = EIO;
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') {
            continue;
        }
        struct stat st;
        char format[260];
        strcpy(format, entry->d_name);
        strcat(format, ":/");
        int sr = stat(format, &st);
        if (sr == 0) {
            if (entry->d_ino == current_ino) {
                format[strlen(format) - 1] = 0;
                if (*buf_ptr >= strlen(format)) {
                    res = 0;
                    *buf_ptr -= strlen(format);
                    (void) buf;
                    memcpy(buf + *buf_ptr, format, strlen(format));
                } else {
                    res = ERANGE;
                }
                break;
            }
        }
    }
    closedir(dir);
    return res;
}

char* getcwd(char* buf, size_t size) {    
    if (size == 0) {
        errno = EINVAL;
        return NULL;
    }

    const char* dots =
        "../../../../../../../../../../../../../../../../"
        "../../../../../../../../../../../../../../../../"
        "../../../../../../../../../../../../../../../../"
        "../../../../../../../../../../../../../../../..";
    const char* dot_ptr = dots + strlen(dots) - 2;

    ino_t current_ino = GetIno(".");
    if (current_ino == 0) {
        errno = EIO;
        return NULL;
    }

    if (buf == NULL) {
        buf = malloc(size);
        if (buf == NULL) {
            errno = ENOMEM;
            return NULL;
        }
    }

    size_t buf_ptr = size - 1;
    buf[buf_ptr] = 0;

    while (true) {
        ino_t new_ino = GetIno(dot_ptr);
        if (new_ino == current_ino) {
            break;
        }

        char* name = GetName(dot_ptr, current_ino);
        if (name == NULL) {
            break;
        }
        if (buf_ptr >= strlen(name) + 1) {
            buf_ptr -= strlen(name) + 1;
            buf[buf_ptr] = '/';
            memcpy(buf + buf_ptr + 1, name, strlen(name));
        } else {
            errno = ERANGE;
            return NULL;
        }
        free(name);

        current_ino = new_ino;
        dot_ptr -= 3;
        if (dot_ptr < dots) {
            errno = ENAMETOOLONG;
            return NULL;
        }
    }

    int res = AddDeviceRoot(buf, &buf_ptr, current_ino);
    if (res != 0) {
        errno = res;
        return NULL;
    }

    memmove(buf, buf + buf_ptr, size - buf_ptr);
    buf[size - buf_ptr] = 0;
    return buf;
}