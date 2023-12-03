#pragma once

#include <common.h>

struct stack_adt;

struct stack_adt* StackAdtCreate(void);
void StackAdtDestroy(struct stack_adt* stack);
void StackAdtPush(struct stack_adt* stack, void* data);
void* StackAdtPeek(struct stack_adt* stack);
void* StackAdtPop(struct stack_adt* stack);
int StackAdtSize(struct stack_adt* stack);