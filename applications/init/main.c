

#include <stdio.h>
#include <string.h>
#include <unistd.h>
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
        if (!strcmp(line, "fork\n")) {
            printf("About to fork()...\n");
            pid_t ret = fork();
            printf("Fork returned %d!\n", ret);
            while (true) {
                ;
            }
        } else if (!strcmp(line, "clear\n")) {
            printf("\x1B[0m");
            fflush(stdout);
        
        } else {
            printf("Command not found: %s\n", line);
        }
    }

    return 0;
}