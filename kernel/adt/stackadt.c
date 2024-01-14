#include <common.h>
#include <linkedlist.h>
#include <stackadt.h>
#include <heap.h>
#include <assert.h>
#include <panic.h>

struct stack_adt {
    struct linked_list* list;
};

struct stack_adt* StackAdtCreate(void) {
    struct stack_adt* stack = AllocHeap(sizeof(struct stack_adt));
    stack->list = ListCreate();
    return stack;
}

void StackAdtDestroy(struct stack_adt* stack) {
    ListDestroy(stack->list);
    FreeHeap(stack);
}

void StackAdtPush(struct stack_adt* stack, void* data) {
    ListInsertStart(stack->list, data);
}

void* StackAdtPeek(struct stack_adt* stack) {
    return ListGetDataFromNode(ListGetFirstNode(stack->list));
}

void* StackAdtPop(struct stack_adt* stack) {
    void* data = StackAdtPeek(stack);
    ListDeleteIndex(stack->list, 0);
    return data;
}

int StackAdtSize(struct stack_adt* stack) {
    return ListSize(stack->list);
}