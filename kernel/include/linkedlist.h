#pragma once

#include <common.h>

struct linked_list;
struct linked_list_node;

export struct linked_list* LinkedListCreate(void);
export void LinkedListInsertEnd(struct linked_list* list, void* data);
export bool LinkedListContains(struct linked_list* list, void* data);
export int LinkedListGetIndex(struct linked_list* list, void* data);
export void* LinkedListGetData(struct linked_list* list, int index);
export bool LinkedListDeleteIndex(struct linked_list* list, int index);
export bool LinkedListDeleteData(struct linked_list* list, void* data);
export int LinkedListSize(struct linked_list* list);
export void LinkedListDestroy(struct linked_list* list);

export struct linked_list_node* LinkedListGetFirstNode(struct linked_list* list);
export struct linked_list_node* LinkedListGetNextNode(struct linked_list_node* prev_node);
export void* LinkedListGetDataFromNode(struct linked_list_node* node);
