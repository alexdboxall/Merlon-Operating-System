
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <timeconv.h>
#include <dirent.h>
#include <time.h>
#include <sys/stat.h>

#include <os/time.h>
#include <os/sysinfo.h>

extern int EditorMain(int argc, char** argv);
extern int LsMain(int argc, char** argv);

static void ShowVersionInformation(void) {
    char ver_string[64];
    int major_ver;
    int minor_ver;
    OsGetVersion(&major_ver, &minor_ver, ver_string, 32);

    printf(
        "%s\n"
        "Version %d.%d\n"
        "Copyright Alex Boxall 2022-2024\n\n",
        ver_string, major_ver, minor_ver
    );
}

static void ShowMemoryUsage(void) {
    size_t free = OsGetFreeMemoryKilobytes();
    size_t total = OsGetTotalMemoryKilobytes();
    if (total == 0) {
        printf("Memory information unavailable\n\n");
    } else {
        printf(
            "%d / %d KB used (%d%% free)\n\n", 
            total - free, total, 100 * (free) / total
        );
    }  
}

static void ShowDateAndTime(void) {
    time_t curtime;
    time(&curtime);
    printf("%s\n", ctime(&curtime));
}

int main(void) {

    /*
    pid_t child = fork();
    if (child == 0) {
        execve("sys:/shell.exe", ...);
        return -1;
    }c
    
    while (true) {
        wait(0);
    }
    */

    putchar('\n');
    ShowVersionInformation();
    ShowMemoryUsage();
    chdir("drv0:/");
    
    char line[300];
    char cwd_buffer[300];
    while (true) {
        getcwd(cwd_buffer, 299);
        printf("%s>", cwd_buffer);
        fflush(stdout);
        fgets(line, 299, stdin);

        errno = 0;

        if (!strcmp(line, "fork\n")) {
            printf("About to fork()...\n");
            pid_t ret = fork();
            printf("Fork returned %d!\n", ret);
            while (true) {
                ;
            }
        
        } else if (!strcmp(line, "clear\n")) {
            printf("\x1B[2J");
            fflush(stdout);

        } else if (!strcmp(line, "sleep_test\n") || !strcmp(line, "sleep test\n")) {
            for (int i = 0; i < 10; ++i) {
                printf("%d...\n", i + 1);
                sleep(1);
            }

        } else if (!strcmp(line, "mem\n") || !strcmp(line, "ram\n")) {
            ShowMemoryUsage();

        } else if (!strcmp(line, "date\n") || !strcmp(line, "time\n")) {
            ShowDateAndTime();

        } else if (!strcmp(line, "ver\n")) {
            ShowVersionInformation();

        } else if (!strcmp(line, "ed\n")) {
            EditorMain(1, NULL);

        } else if (!strcmp(line, "pwd\n")) {
            printf("%s\n\n", cwd_buffer);

        } else if (!strcmp(line, "ls\n") || !strcmp(line, "dir\n")) {
            LsMain(1, NULL);

        } else if (line[0] == 'c' && line[1] == 'd' && line[2] == ' ') {
            line[strlen(line) - 1] = 0;
            int res = chdir(line + 3);
            if (res == -1) {
                printf("%s\n\n", strerror(errno));
            }

        } else {
            printf("Command not found: %s\n", line);
        }
    }

    return 0;
}