#include <common.h>
#include <threadlist.h>
#include <heap.h>
#include <assert.h>
#include <thread.h>
#include <panic.h>
#include <string.h>

void ThreadListInit(struct thread_list* list) {
    memset(list, 0, sizeof(struct thread_list));
}

void ThreadListInsert(struct thread_list* list, struct thread* thread) {
#ifndef NDEBUG
    assert(!ThreadListContains(list, thread))
#endif
    if (list->tail == NULL) {
        assert(list->head == NULL);
        list->head = thread;

    } else {
        list->tail->next = thread;
    }

    list->tail = thread;
    thread->next = NULL;
}

static int ThreadListGetIndex(struct thread_list* list, struct thread* thread) {
    struct thread* iter = list->head;
    int i = 0;
    while (iter != NULL) {
        if (iter == thread) {
            return i;
        }
        ++i;
        iter = iter->next;
    }
    return -1;
}

bool ThreadListContains(struct thread_list* list, struct thread* thread) {
    return ThreadListGetIndex(list, thread) != -1;
}

static void ProperDelete(struct thread_list* list, struct thread* iter, struct thread* prev) {
    if (iter == list->head) {
        list->head = list->head->next;
    } else {
        prev->next = iter->next;
    }

    if (iter == list->tail) {
        list->tail = prev;
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
        iter = iter->next;
    }

    Panic(PANIC_LINKED_LIST);
}

void ThreadListDeleteTop(struct thread_list* list) {
    if (list->head == list->tail) {
        list->tail = NULL;
    }
    list->head = list->head->next;
}

void ThreadListDelete(struct thread_list* list, struct thread* thread) {
    ThreadListDeleteIndex(list, ThreadListGetIndex(list, thread));
}
