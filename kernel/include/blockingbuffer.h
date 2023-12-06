#pragma once

#include <common.h>

struct blocking_buffer;

struct blocking_buffer* BlockingBufferCreate(int size);
void BlockingBufferDestroy(struct blocking_buffer* buffer);
int BlockingBufferAdd(struct blocking_buffer* buffer, uint8_t c, bool block);
uint8_t BlockingBufferGet(struct blocking_buffer* buffer);
int BlockingBufferTryGet(struct blocking_buffer* buffer, uint8_t* c);