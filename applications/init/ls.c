
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>

static const char* GetDirectoryEntryTypeString(int dirent) {
    switch (dirent) {
        case DT_BLK : return "<BLK>";
        case DT_CHR : return "<CHR>";
        case DT_DIR : return "<DIR>";
        case DT_FIFO: return "<FIFO>";
        case DT_LNK : return "<LNK>";
        case DT_REG : return "";
        case DT_SOCK: return "<SOCK>";
        default     : return "<\?\?\?>";  /* Avoid trigraph */
    }
}

int LsMain(int, char**) {
    DIR *d;
    struct dirent *dir;
    d = opendir(".");
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            printf(
                "%8s %s\n",
                GetDirectoryEntryTypeString(dir->d_type),
                dir->d_namecd
            );
        }
        closedir(d);
        printf("\n");
    } else {
        printf("%s\n\n", strerror(errno));
    }
    return 0;
}