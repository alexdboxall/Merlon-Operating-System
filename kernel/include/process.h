#pragma once

#include <common.h>
#include <sys/types.h>

struct process;

void InitProcess(void);
struct process* CreateProcess(pid_t parent_pid);
struct process* ForkProcess(void);
pid_t WaitProcess(pid_t pid, int* status, int flags);
void KillProcess(int retv);

struct process* GetProcessFromPid(pid_t pid);
struct process* GetProcess(void);
pid_t GetPid(struct process* prcss);

void AddThreadToProcess(struct process* prcss, struct thread* thr);
struct process* CreateProcessWithEntryPoint(pid_t parent, void(*entry_point)(void*), void* arg);