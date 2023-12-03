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
    stack->list = LinkedListCreate();
    return stack;
}

void StackAdtDestroy(struct stack_adt* stack) {
    LinkedListDestroy(stack->list);
    FreeHeap(stack);
}

void StackAdtPush(struct stack_adt* stack, void* data) {
    LinkedListInsertStart(stack->list, data);
}

void* StackAdtPeek(struct stack_adt* stack) {
    return LinkedListGetDataFromNode(LinkedListGetFirstNode(stack->list));
}

void* StackAdtPop(struct stack_adt* stack) {
    void* data = StackAdtPeek(stack);
    LinkedListDeleteIndex(stack->list, 0);
    return data;
}

int StackAdtSize(struct stack_adt* stack) {
    return LinkedListSize(stack->list);
}