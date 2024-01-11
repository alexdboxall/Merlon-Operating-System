
#include <fcntl.h>
#include <unistd.h>
#include <sched.h>

void _start() {
    //sched_yield();

    int stdout = open("con:", O_WRONLY, 0);
    char msg[] = "Wahoo!! Shared libraries seem to work!";
    write(stdout, msg, sizeof(msg));
    write(stdout, msg, 7);
    close(stdout);

    while (1) {
        ;
    }
}