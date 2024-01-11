#include "include/font.h"
#include "include/util.h"
#include "include/bayer.h"
#include "include/lowvga.h"
#include <string.h>
#include <byteswap.h>

/*
 * Bounds checking width and height, etc. is the responsiblity of the calling driver. There is a function in 
 * util.c that can do that for you.
 */
void GenericVideoPutrect(uint8_t* virtual_framebuffer, int pitch, int depth, int x, int y, int w, int h, uint32_t colour) {
    uint8_t* position = virtual_framebuffer + y * pitch;
    
    if (depth == 32) {
        uint32_t* pos32 = ((uint32_t*) (size_t) position) + x;
        uint8_t* start_pos = (uint8_t*) pos32;

#ifdef ARCH_BIG_ENDIAN
        colour = bswap_32(colour);
#endif

        for (int i = 0; i < w; ++i) {
            *pos32++ = colour;
        }

        for (int i = 1; i < h; ++i) {
            memcpy(start_pos + pitch * i, start_pos, 4 * w);
        }

    } else if (depth == 24) {
        uint8_t b = (colour >> 0) & 0xFF;
        uint8_t g = (colour >> 8) & 0xFF;
        uint8_t r = (colour >> 16) & 0xFF;
    
        position += x * 3;

        uint8_t* start_pos = position;

        for (int i = 0; i < w; ++i) {
            *position++ = b;
            *position++ = g;
            *position++ = r;
        }
    
        for (int i = 1; i < h; ++i) {
            memcpy(start_pos + pitch * i, start_pos, 3 * w);
        }

    } else if (depth == 15 || depth == 16) {
        uint16_t* pos16 = ((uint16_t*) (size_t) position) + x;
        uint8_t* start_pos = (uint8_t*) pos16;
        uint16_t mod_colour = ConvertToColourDepth(colour, depth);

#ifdef ARCH_BIG_ENDIAN
        mod_colour = bswap_16(mod_colour);
#endif

        for (int i = 0; i < w; ++i) {
            *pos16++ = mod_colour;
        }
        
        for (int i = 1; i < h; ++i) {
            memcpy(start_pos + pitch * i, start_pos, 2 * w);
        }

    } else {
        uint8_t* start_pos = position + x;

        bool exact_match = IsExactVgaMatch(colour, depth == 8);

        if (!exact_match) {
            for (int j = 0; j < h; ++j) {
                position = start_pos;
                start_pos += pitch;
                for (int i = 0; i < w; ++i) {
                    uint32_t bayer = GetBayerAdjustedColour8(x + i, y + j, colour);
                    uint32_t mod_colour = ConvertToColourDepth(bayer, depth);
                    *position++ = mod_colour & 0xFF;
                }
            }
        } else {
            uint8_t mod_colour = ConvertToColourDepth(colour, depth);
            for (int j = 0; j < h; ++j) {
                memset(start_pos, mod_colour, w);
                start_pos += pitch;
            }
        }
    }
}

void GenericVideoPutpixel(uint8_t* virtual_framebuffer, int pitch, int depth, int x, int y, uint32_t colour) {
    uint8_t* position = virtual_framebuffer + y * pitch;
    if (depth == 24) {
        position += x * 3;
        *position++ = (colour >> 0) & 0xFF;
        *position++ = (colour >> 8) & 0xFF;
        *position++ = (colour >> 16) & 0xFF;

    } else if (depth == 32) {
        position += x * 4;
        *position++ = (colour >> 0) & 0xFF;
        *position++ = (colour >> 8) & 0xFF;
        *position++ = (colour >> 16) & 0xFF;

    } else if (depth == 15 || depth == 16) {
        position += x * 2;
        uint32_t mod_colour = ConvertToColourDepth(colour, depth);
        *position++ = (mod_colour >> 0) & 0xFF;
        *position++ = (mod_colour >> 8) & 0xFF;

    } else {
        bool exact_match = IsExactVgaMatch(colour, depth == 8);
        position += x;

        uint32_t mod_colour;
        if (!exact_match) {
            uint32_t bayer_colour = GetBayerAdjustedColour8(x, y, colour);
            mod_colour = ConvertToColourDepth(bayer_colour, depth);

        } else {
            mod_colour = ConvertToColourDepth(colour, depth);
        }
        *position++ = mod_colour & 0xFF;
    }
}

void GenericVideoDrawConsoleCharacter(uint8_t* virtual_framebuffer, int pitch, int depth, int pixel_x, int pixel_y, uint32_t bg, uint32_t fg, char c) {
    for (int i = 0; i < 16; ++i) {
        uint8_t font_char = CodePage437Font[((uint8_t) c) * 16 + i];

        for (int j = 0; j < 8; ++j) {
            uint32_t colour = (font_char & 0x80) ? fg : bg;
            font_char <<= 1;

            GenericVideoPutpixel(virtual_framebuffer, pitch, depth, pixel_x + j, pixel_y + i, colour);
        }
    }
}
