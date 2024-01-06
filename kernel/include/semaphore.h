#pragma once

#include <common.h>

struct semaphore;
struct thread;

#define SEM_BIG_NUMBER    (1 << 30)

#define SEM_DONT_CARE     0
#define SEM_REQUIRE_ZERO  1
#define SEM_REQUIRE_FULL  2

struct semaphore* CreateSemaphore(const char* name, int max_count, int initial_count);
int AcquireSemaphore(struct semaphore* sem, int timeout_ms);
void ReleaseSemaphore(struct semaphore* sem);
int DestroySemaphore(struct semaphore* sem, int mode);
int GetSemaphoreCount(struct semaphore* sem);

#define CreateMutex(name) CreateSemaphore(name, 1, 0)
#define AcquireMutex(mtx, timeout_ms) AcquireSemaphore(mtx, timeout_ms)
#define ReleaseMutex(mtx) ReleaseSemaphore(mtx)
#define DestroyMutex(mtx) DestroySemaphore(mtx, SEM_REQUIRE_ZERO)

void CancelSemaphoreOfThread(struct thread* thr);