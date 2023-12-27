#pragma once

#include <common.h>

int GetApproxDistanceBetweenColours(uint32_t a, uint32_t b);
uint32_t HsvToRgb(int h, int s, int v);
uint32_t ConvertToColourDepth(uint32_t colour, int bits);
void ClipRectToScreenBounds(int x, int y, int* w, int* h, int screen_width, int screen_height);