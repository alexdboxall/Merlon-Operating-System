#pragma once

#include <common.h>

struct linked_list;
struct linked_list_node;

struct linked_list* ListCreate(void);
void ListInsertStart(struct linked_list* list, void* data);
void ListInsertEnd(struct linked_list* list, void* data);
bool ListContains(struct linked_list* list, void* data);
int ListGetIndex(struct linked_list* list, void* data);
void* ListGetData(struct linked_list* list, int index);
bool ListDeleteIndex(struct linked_list* list, int index);
bool ListDeleteData(struct linked_list* list, void* data);
int ListSize(struct linked_list* list);
void ListDestroy(struct linked_list* list);

struct linked_list_node* ListGetFirstNode(struct linked_list* list);
struct linked_list_node* ListGetNextNode(struct linked_list_node* prev_node);
void* ListGetDataFromNode(struct linked_list_node* node);
