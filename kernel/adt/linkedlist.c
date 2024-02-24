
/*
 * adt/list.c - Linked Lists
 *
 * Implements a generic linked list data structure.
 */

#include <common.h>
#include <linkedlist.h>
#include <heap.h>
#include <assert.h>
#include <panic.h>

struct linked_list_node {
    void* data;
    struct linked_list_node* next;
};

struct linked_list {
    int size;
    struct linked_list_node* head;
    struct linked_list_node* tail;
};

struct linked_list* ListCreate(void) {
    return AllocHeapZero(sizeof(struct linked_list));
}

void ListInsertStart(struct linked_list* list, void* data) {
    struct linked_list_node* node = AllocHeap(sizeof(struct linked_list_node));
    node->data = data;
    node->next = list->tail;
    
    if (list->head == NULL) {
        assert(list->tail == NULL);
        list->tail = node;
    }

    list->head = node;
    list->size++;
}

void ListInsertEnd(struct linked_list* list, void* data) {
    if (list->tail == NULL) {
        assert(list->head == NULL);
        list->tail = AllocHeap(sizeof(struct linked_list_node));
        list->head = list->tail;

    } else {
        list->tail->next = AllocHeap(sizeof(struct linked_list_node));
        list->tail = list->tail->next;
    }

    list->tail->data = data;
    list->tail->next = NULL;
    list->size++;
}

bool ListContains(struct linked_list* list, void* data) {
    return ListGetIndex(list, data) != -1;
}

int ListGetIndex(struct linked_list* list, void* data) {
    struct linked_list_node* iter = list->head;
    int i = 0;
    while (iter != NULL) {
        if (iter->data == data) {
            return i;
        }
        ++i;
        iter = iter->next;
    }
    return -1;
}
 
void* ListGetData(struct linked_list* list, int index) {
    struct linked_list_node* iter = list->head;
    int i = 0;
    while (iter != NULL) {
        if (i == index) {
            return iter->data;
        }
        ++i;
        iter = iter->next;
    }
    Panic(PANIC_LINKED_LIST);
}

static void ProperDelete(struct linked_list* list, struct linked_list_node* iter, struct linked_list_node* prev) {
    if (iter == list->head) {
        list->head = list->head->next;
    } else {
        prev->next = iter->next;
    }

    if (iter == list->tail) {
        list->tail = prev;
    }

    FreeHeap(iter);
    list->size--;
}

bool ListDeleteIndex(struct linked_list* list, int index) {
    if (index >= list->size || index < 0) {
        return false;
    }

    struct linked_list_node* iter = list->head;
    struct linked_list_node* prev = NULL;

    int i = 0;
    while (iter != NULL) {
        if (i == index) {
            ProperDelete(list, iter, prev);
            return true;
        }
        ++i;
        prev = iter;
        iter = iter->next;
    }

    return false;
}

bool ListDeleteData(struct linked_list* list, void* data) {
    return ListDeleteIndex(list, ListGetIndex(list, data));
}

int ListSize(struct linked_list* list) {
    return list->size;
}

void ListDestroy(struct linked_list* list) {
    while (list->size > 0) {
        ListDeleteIndex(list, 0);
    }
    FreeHeap(list);
}

struct linked_list_node* ListGetFirstNode(struct linked_list* list) {
    if (list == NULL) {
        Panic(PANIC_LINKED_LIST);
    }
    return list->head;
}

struct linked_list_node* ListGetNextNode(struct linked_list_node* prev_node) {
    if (prev_node == NULL) {
        Panic(PANIC_LINKED_LIST);
    }
    return prev_node->next;
}

void* ListGetDataFromNode(struct linked_list_node* node) {
    if (node == NULL) {
        Panic(PANIC_LINKED_LIST);
    }
    return node->data;
}

void* ListGetDataAtIndex(struct linked_list* list, int index) {
    struct linked_list_node* iter = list->head;
    while (index > 0) {
        --index;
        if (iter == NULL) {
            return NULL;
        }
        iter = iter->next;
    }
    if (iter == NULL) {
        return NULL;
    } else {
        return iter->data;
    }
}