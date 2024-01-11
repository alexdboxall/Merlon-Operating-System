
#include <fcntl.h>
#include <unistd.h>
#include <sched.h>
#include <stdlib.h>

void _start() {
    int stdout = open("con:", O_WRONLY, 0);
    char msg[] = "Wahoo!! Shared libraries seem to work!";
    write(stdout, msg, sizeof(msg));
    write(stdout, msg, 7);
    close(stdout);

    //_exit(0);

    while (1) {
        sched_yield();
    }
}