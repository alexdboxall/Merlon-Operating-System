

#include <stdio.h>
#include <stdbool.h>

int main(void) {
    /*
    pid_t child = fork();
    if (child == 0) {
        execve("sys:/shell.exe", ...);
        return -1;
    }
    
    while (true) {
        wait(0);
    }
    */
    
    char line[300];
    while (true) {
        printf("drv0:/>");
        fflush(stdout);
        fgets(line, 299, stdin);
        printf("Command not found: %s\n", line);
    }

    return 0;
}