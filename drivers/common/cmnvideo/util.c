#include <common.h>
#include "include/lowvga.h"

int GetApproxDistanceBetweenColours(uint32_t a, uint32_t b) {
    int r1 = (a >> 16) & 0xFF;
    int r2 = (b >> 16) & 0xFF;
    int g1 = (a >> 8) & 0xFF;
    int g2 = (b >> 8) & 0xFF;
    int b1 = a & 0xFF;
    int b2 = b & 0xFF;

    int rd = r1 - r2;
    int gd = g1 - g2;
    int bd = b1 - b2;

    int rmean = (r1 + r2) / 2;

    // from here:
    // https://www.compuphase.com/cmetric.htm
    return (rd * rd * (512 + rmean)) 
            + (gd * gd * 1024)
            + (bd * bd * (767 - rmean));
}

uint32_t HsvToRgb(int H, int S, int V) {
	int r = 0;
    int g = 128;
    int b = 0;
	
	int s = S * 1000 / 100;
	int v = V * 1000 / 100;
	
	int f = (H % 60) * 1000 / 60;

	int p = v * (1000 - s) / 1000;
	int q = v * (1000 - f * s / 1000) / 1000;
	int t = v * (1000 - (1000 - f) * s / 1000) / 1000;
	
	switch (H / 60) {
		case 0: r = v, g = t, b = p; break;
		case 1: r = q, g = v, b = p; break;
		case 2: r = p, g = v, b = t; break;
		case 3: r = p, g = q, b = v; break;
		case 4: r = t, g = p, b = v; break;
		case 5: r = v, g = p, b = q; break;
	}
	
	r = (r * 255 / 1000) & 0xFF;
	g = (g * 255 / 1000) & 0xFF;
	b = (b * 255 / 1000) & 0xFF;
	
	return (r << 16) | (g << 8) | b;
}


uint32_t ConvertToColourDepth(uint32_t colour, int bits) {
    if (bits > 16) return colour;

    uint8_t r = (colour >> 16) & 0xFF;
    uint8_t g = (colour >> 8) & 0xFF;
    uint8_t b = (colour >> 0) & 0xFF;

    if (bits == 16) {
        uint32_t nr = r >> 3;
        uint32_t ng = g >> 2;
        uint32_t nb = b >> 3;
        return (nr << 11) | (ng << 5) | nb;

    } else if (bits == 15) {
        uint32_t nr = r >> 3;
        uint32_t ng = g >> 3;
        uint32_t nb = b >> 3;
        return (nr << 10) | (ng << 5) | nb;

    } else {
        uint32_t nr = r >> 5;
        uint32_t ng = g >> 5;
        uint32_t nb = b >> 5;

        uint32_t ninebit = (nr << 6) | (ng << 3) | nb;
        if (bits == 8) {
            return Convert9BitToVga256(ninebit);
        } else if (bits == 4) {
            return Convert9BitToVga16(ninebit);
        }

        return 0;
    }
}

void ClipRectToScreenBounds(int x, int y, int* w, int* h, int screen_width, int screen_height) {
    if (x < 0) {
        *w += x;
        x = 0;
        if (*w < 0) {
            *w = 0;
        }
    }
    if (y < 0) {
        *h += y;
        y = 0;
        if (*h < 0) {
            *h = 0;
        }
    }
    if (x + *w > screen_width) {
        *w = screen_width - x;
        if (*w < 0) {
            *w = 0;
        }
    }
    if (y + *h > screen_height) {
        *h = screen_height - x;
        if (*h < 0) {
            *h = 0;
        }
    }
}