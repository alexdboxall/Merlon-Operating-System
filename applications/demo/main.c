
#include <fcntl.h>
#include <unistd.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

void _start(int argc, char** argv, char** envp) {
    stdin = fopen("con:", "r");
    stdout = fopen("con:", "w");
    stderr = fopen("con:", "w");
    setvbuf(stderr, NULL, _IONBF, 1);

    errno = 0;

    (void) argc;
    (void) argv;
    (void) envp;

    FILE* s = fopen("con:", "w");
    fprintf(s, "Wahoo!! Shared libraries seem to work!\n");
    fclose(s);

    printf("argc = %d\n", argc);
    for (int i = 0; i < argc; ++i) {
        printf("argv[%d] = %s\n", i, argv[i] == NULL ? "NULL" : argv[i]);
    }

    //_exit(0);

    while (1) {
        sched_yield();
    }
}