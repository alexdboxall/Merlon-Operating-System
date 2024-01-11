
#include <fcntl.h>
#include <unistd.h>

void _start() {
    int stdout = open("con:", O_WRONLY, 0);
    char msg[] = "Hello world from usermode!";
    write(stdout, msg, sizeof(msg));
    close(stdout);

    while (1) {
        ;
    }
}