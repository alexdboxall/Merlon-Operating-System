
/***
 * thread/idle.c - System Idle Task
 * 
 * A thread that is run if not other thread is available to run. The idle thread must therefore
 * never block.
 */

#include <arch.h>
#include <thread.h>
#include <virtual.h>
#include <irql.h>

static void IdleThread(void*) {
    while (1) {
        ArchStallProcessor();
    }
}

void InitIdle(void) {
    CreateThreadEx(IdleThread, NULL, GetVas(), "idle thread", NULL, SCHEDULE_POLICY_FIXED, FIXED_PRIORITY_IDLE, 0);
}