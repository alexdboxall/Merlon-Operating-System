
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>

int remove(const char* path) {
    struct stat st;
    int res = stat(path, &st);
    if (res != 0) {
        return res;
    }
    if ((st.st_mode & S_IFMT) == S_IFDIR) {
        return rmdir(path);
    } else {
        return unlink(path);
    }
}
