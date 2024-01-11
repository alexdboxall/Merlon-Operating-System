
#include <unistd.h>
#include <fcntl.h>
#include <sched.h>
#include "krnlapi.h"

/*
 * This is where the ARGV and ENVP arrays are. The data is stored in 
 * `argvenp_data`.
 * 
 * e.g. a potential example of that this array migh hold.
 *      argvenvp_arrays[0] = argv[0] = argvenp_data + 0
 *      argvenvp_arrays[1] = argv[1] = argvenp_data + 156
 *      argvenvp_arrays[2] = argv[2] = argvenp_data + 522
 *      argvenvp_arrays[3] = NULL
 *      argvenvp_arrays[4] = envp[0] = argvenp_data + 622
 *      argvenvp_arrays[5] = envp[1] = argvenp_data + 1411
 *      argvenvp_arrays[6] = NULL
 */
char* argvenvp_arrays[512];
int array_ptr = 0;
char** envp_start = NULL;

/* 
 * `argvenvp_arrays` has entries which point in here.
 */
char argvenp_data[1024 * 64];
int data_ptr = 0;

/*static*/ void found_argvenvp(char* data) {
    argvenvp_arrays[array_ptr++] = argvenp_data + data_ptr;
    argvenvp_arrays[array_ptr] = 0;
    xstrcpy(argvenp_data + data_ptr, data);
    data_ptr += xstrlen(data) + 1;
}

static void start_envp(void) {
    argvenvp_arrays[array_ptr++] = 0;
    argvenvp_arrays[array_ptr] = 0;
    envp_start = argvenvp_arrays + array_ptr;
}

void loader_main(void*) {
    char* filename = "sys:/demo.exe";

    // find all argvs, call found_argvenvp(...) on each
    start_envp();
    // find all envps, call found_argvenvp(...) on each

    execve(filename, argvenvp_arrays, envp_start);

    while (true) {
        sched_yield();
    }
}