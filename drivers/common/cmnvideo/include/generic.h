#pragma once

#include <common.h>

void GenericVideoPutpixel(uint8_t* virtual_framebuffer, int pitch, int depth, int x, int y, uint32_t colour);
void GenericVideoDrawConsoleCharacter(uint8_t* virtual_framebuffer, int pitch, int depth, int pixel_x, int pixel_y, uint32_t bg, uint32_t fg, char c);
void GenericVideoPutrect(uint8_t* virtual_framebuffer, int pitch, int depth, int x, int y, int w, int h, uint32_t colour);
