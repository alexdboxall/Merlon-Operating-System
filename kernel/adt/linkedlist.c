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

struct linked_list* LinkedListCreate(void) {
    struct linked_list* list = AllocHeap(sizeof(struct linked_list));
    list->size = 0;
    list->head = NULL;
    list->tail = NULL;
    return list;
}

void LinkedListInsertEnd(struct linked_list* list, void* data) {
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

bool LinkedListContains(struct linked_list* list, void* data) {
    return LinkedListGetIndex(list, data) != -1;
}

int LinkedListGetIndex(struct linked_list* list, void* data) {
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
 
void* LinkedListGetData(struct linked_list* list, int index) {
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

bool LinkedListDeleteIndex(struct linked_list* list, int index) {
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

bool LinkedListDeleteData(struct linked_list* list, void* data) {
    return LinkedListDeleteIndex(list, LinkedListGetIndex(list, data));
}

int LinkedListSize(struct linked_list* list) {
    return list->size;
}

void LinkedListDestroy(struct linked_list* list) {
    while (list->size > 0) {
        LinkedListDeleteIndex(list, 0);
    }
    FreeHeap(list);
}

struct linked_list_node* LinkedListGetFirstNode(struct linked_list* list) {
    return list->head;
}

struct linked_list_node* LinkedListGetNextNode(struct linked_list_node* prev_node) {
    if (prev_node == NULL) {
        Panic(PANIC_LINKED_LIST);
    }
    return prev_node->next;
}

void* LinkedListGetDataFromNode(struct linked_list_node* node) {
    if (node == NULL) {
        Panic(PANIC_LINKED_LIST);
    }
    return node->data;
}