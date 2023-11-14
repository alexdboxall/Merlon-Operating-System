#pragma once

#ifdef NDEBUG

#else

#include <common.h>

#define DBGPKT_TFW     0

void DbgWritePacket(int type, uint8_t* data, int size);
void DbgReadPacket(int* type, uint8_t* data, int* size);

#endif