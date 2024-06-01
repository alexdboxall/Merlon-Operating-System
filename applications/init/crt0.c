
#include <unistd.h>
#include <sched.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <stdbool.h>
#include <locale.h>
#include <os/time.h>

int main(int argc, char** argv, char** envp);
void OsInitSignals(void);

extern uint64_t initial_time_for_clock;

void _start(int argc, char** argv, char** envp) {
    initial_time_for_clock = OsGetLocalTime();
    OsInitSignals();

    stdin = fopen("con:", "r");
    stdout = fopen("con:", "w");
    stderr = fopen("con:", "w");
    setvbuf(stderr, NULL, _IONBF, 1);
    setlocale(LC_ALL, "C");

    errno = 0;
    exit(main(argc, argv, envp));

    while (1) {
        sched_yield();
    }
}