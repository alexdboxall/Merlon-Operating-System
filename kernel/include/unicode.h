#pragma once

#include <common.h>

int Utf16ToCodepoints(uint16_t* utf16, uint32_t* codepoints, int in_length, int* out_length);
int Utf8ToCodepoints(uint8_t* utf8, uint32_t* codepoints, int in_length, int* out_length);

int CodepointsToUtf16(uint32_t* codepoints, uint16_t* utf16, int in_length, int* out_length);
int CodepointsToUtf8(uint32_t* codepoints, uint8_t* utf8, int in_length, int* out_length);