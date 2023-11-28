#pragma once

#include <stdbool.h>

struct thread;

struct thread_list {
    struct thread* head;
    struct thread* tail;
};

void ThreadListInit(struct thread_list* list);
void ThreadListInsert(struct thread_list* list, struct thread* thread);
bool ThreadListContains(struct thread_list* list, struct thread* thread);
void ThreadListDelete(struct thread_list* list, struct thread* thread);
void ThreadListDeleteTop(struct thread_list* list);