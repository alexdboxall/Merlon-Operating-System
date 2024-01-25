
/*
 * adt/threadlist.c - Thread-Specific Linked Lists
 *
 * An allocation-free linked list implementation designed specifically for 
 * `struct thread`. Required by the scheduler.
 */

#include <common.h>
#include <threadlist.h>
#include <heap.h>
#include <assert.h>
#include <thread.h>
#include <panic.h>
#include <log.h>
#include <string.h>

void ThreadListInit(struct thread_list* list, int index) {
    inline_memset(list, 0, sizeof(struct thread_list));
    list->index = index;
}

void ThreadListInsert(struct thread_list* list, struct thread* thread) {
#ifndef NDEBUG
    if (ThreadListContains(list, thread)) {
        assert(!ThreadListContains(list, thread));
    }
#endif

    if (list->tail == NULL) {
        assert(list->head == NULL);
        list->head = thread;

    } else {
        list->tail->next[list->index] = thread;
    }

    list->tail = thread;
    thread->next[list->index] = NULL;
}

static int ThreadListGetIndex(struct thread_list* list, struct thread* thread) {
    struct thread* iter = list->head;
    int i = 0;
    while (iter != NULL) {
        if (iter == thread) {
            return i;
        }
        ++i;
        iter = iter->next[list->index];
    }
    return -1;
}

bool ThreadListContains(struct thread_list* list, struct thread* thread) {
    return ThreadListGetIndex(list, thread) != -1;
}

static void ProperDelete(struct thread_list* list, struct thread* iter, struct thread* prev) {
    if (iter == list->head) {
        list->head = list->head->next[list->index];
    } else {
        prev->next[list->index] = iter->next[list->index];
    }

    if (iter == list->tail) {
        list->tail = prev;
    }

    if (list->head == NULL) {
        assert(list->tail == NULL);
    }
}

static void ThreadListDeleteIndex(struct thread_list* list, int index) {
    struct thread* iter = list->head;
    struct thread* prev = NULL;

    assert(index >= 0);

    int i = 0;
    while (iter != NULL) {
        if (i == index) {
            ProperDelete(list, iter, prev);
            return;
        }
        ++i;
        prev = iter;
        iter = iter->next[list->index];
    }

    Panic(PANIC_THREAD_LIST);
}

struct thread* ThreadListDeleteTop(struct thread_list* list) {
    struct thread* top = list->head;
    if (list->head == list->tail) {
        list->tail = NULL;
    }
    list->head = list->head->next[list->index];

    if (list->head == NULL) {
        assert(list->tail == NULL);
    }

    return top;
}

void ThreadListDelete(struct thread_list* list, struct thread* thread) {
    ThreadListDeleteIndex(list, ThreadListGetIndex(list, thread));
}
