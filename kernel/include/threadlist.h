#pragma once

#include <common.h>

struct thread;

struct thread_list {
    struct thread* head;
    struct thread* tail;
    int index;
};

void ThreadListInit(struct thread_list* list, int index);
void ThreadListInsert(struct thread_list* list, struct thread* thread);
void ThreadListInsertAtFront(struct thread_list* list, struct thread* thread);
bool ThreadListContains(struct thread_list* list, struct thread* thread);
void ThreadListDelete(struct thread_list* list, struct thread* thread);
struct thread* ThreadListDeleteTop(struct thread_list* list);
