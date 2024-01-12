

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>

int main(int argc, char** argv) {
    (void) argc;
    (void) argv;

    /*
    TODO: one we get `fork` working (and an execve that actually uses the
    filename, we should move the shell code to shell.exe, and just do this in
    init.exe:

    pid_t child = fork();
    if (child == 0) {
        execve("sys:/shell.exe", ...);
        return -1;
    }
    
    while (true) {
        wait(0);
    }
    */

    while (true) {
        printf("drv0:/> ");
        fflush(stdout);

        char line[300];
        fgets(line, 299, stdin);
        printf("Command not found: %s\n", line);
    }

    return 0;
}