#pragma once

#include <stdbool.h>

struct linked_list;
struct linked_list_node;

struct linked_list* LinkedListCreate(void);
void LinkedListInsertEnd(struct linked_list* list, void* data);
bool LinkedListContains(struct linked_list* list, void* data);
int LinkedListGetIndex(struct linked_list* list, void* data);
void* LinkedListGetData(struct linked_list* list, int index);
bool LinkedListDeleteIndex(struct linked_list* list, int index);
bool LinkedListDeleteData(struct linked_list* list, void* data);
int LinkedListSize(struct linked_list* list);
void LinkedListDestroy(struct linked_list* list);

struct linked_list_node* LinkedListGetFirstNode(struct linked_list* list);
struct linked_list_node* LinkedListGetNextNode(struct linked_list_node* prev_node);
void* LinkedListGetDataFromNode(struct linked_list_node* node);
