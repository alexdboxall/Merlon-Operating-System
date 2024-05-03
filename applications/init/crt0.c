
#include <unistd.h>
#include <sched.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <stdbool.h>

extern int main(int argc, char** argv, char** envp);

void _start(int argc, char** argv, char** envp) {
    stdin = fopen("con:", "r");
    stdout = fopen("con:", "w");
    stderr = fopen("con:", "w");
    setvbuf(stderr, NULL, _IONBF, 1);

    errno = 0;

    // TODO: exit() instead of _exit
    _exit(main(argc, argv, envp));

    while (1) {
        sched_yield();
    }
}