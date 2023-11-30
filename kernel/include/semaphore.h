#pragma once

#include <common.h>

struct semaphore;

struct semaphore* CreateSemaphore(int max_count);
int AcquireSemaphore(struct semaphore* sem, int timeout_ms);
void ReleaseSemaphore(struct semaphore* sem);
void DestroySemaphore(struct semaphore* sem);