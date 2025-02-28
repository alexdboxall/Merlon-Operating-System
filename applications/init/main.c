
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <timeconv.h>
#include <dirent.h>
#include <time.h>
#include <signal.h>
#include <sys/stat.h>

#include <merlon/time.h>
#include <merlon/sysinfo.h>

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

void alarm_handler(int sig) {
    (void) sig;
    printf("Got the alarm!\n");
}

static void AlarmTest(void) {
    printf("Testing `alarm()` and `pause()`\n");
    signal(SIGALRM, alarm_handler);
    printf("In 3 seconds, we should receive the alarm.\n");
    alarm(3);
    pause();
    printf("That's all!\n\n");
}

void div0_handler(int) {
    printf("Uh oh, we divided by zero!!\n");
    abort();
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

    tcsetpgrp(fileno(stdin), getpgid(0));
    tcsetpgrp(fileno(stdout), getpgid(0));

    signal(SIGFPE, div0_handler);

    int fpe_test = 3;

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
            fflush(stdout);
            pid_t ret = fork();
            printf("Fork returned %d!\n", ret);
            fflush(stdout);
        
        } else if (!strcmp(line, "clear\n")) {
            printf("\x1B[2J");
            fflush(stdout);

        } else if (!strcmp(line, "alarm_test\n") || !strcmp(line, "alarm test\n")) {
            AlarmTest();

        } else if (!strcmp(line, "sleep_test\n") || !strcmp(line, "sleep test\n")) {
            for (int i = 0; i < 10; ++i) {
                printf("%d...\n", i + 1);
                fflush(stdout);
                sleep(1);
            }

        } else if (!strcmp(line, "div0\n")) {
            int res = 10 / fpe_test;
            printf("Dividing 10 by %d gives %d. Next time, we'll divide by %d.\n\n",
                fpe_test, res, fpe_test - 1
            );
            --fpe_test;

        } else if (!strcmp(line, "mem\n") || !strcmp(line, "ram\n")) {
            ShowMemoryUsage();

        } else if (!strcmp(line, "date\n") || !strcmp(line, "time\n")) {
            ShowDateAndTime();

        } else if (!strcmp(line, "ver\n")) {
            ShowVersionInformation();

        } else if (!strcmp(line, "do_sigill\n") || !strcmp(line, "do sigill\n") || !strcmp(line, "sigill\n")) {
            asm volatile ("hlt");

        } else if (!strcmp(line, "do_sigsegv\n") || !strcmp(line, "do sigsegv\n") || !strcmp(line, "sigsegv\n")) {
            uint32_t* arr = (uint32_t*) (size_t) (0xABCDEF);
            *arr = 0;

        } else if (!strcmp(line, "ed\n")) {
            EditorMain(1, NULL);

        } else if (!strcmp(line, "pwd\n")) {
            printf("%s\n\n", cwd_buffer);

        } else if (!strcmp(line, "exec\n")) {
            char* args[] = {
                "drv0:/System/init.exe",
                NULL
            };
            char* env[] = {
                NULL
            };
            execve(args[0], args, env);

        } else if (!strcmp(line, "ls\n") || !strcmp(line, "dir\n")) {
            LsMain(1, NULL);

        } else if (line[0] == 'c' && line[1] == 'd' && line[2] == ' ') {
            line[strlen(line) - 1] = 0;
            int res = chdir(line + 3);
            if (res == -1) {
                printf("%s\n\n", strerror(errno));
            }

        } else if (strcmp(line, "\n")) {
            printf("Command not found: %s\n", line);
        }
    }

    return 0;
}