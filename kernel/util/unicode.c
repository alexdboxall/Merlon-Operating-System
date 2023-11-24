
#include <unicode.h>
#include <errno.h>

/*
 * Converts UTF16 to UTF32. out_length is in/out - it must contain the output buffer's maximum length,
 * but will receive the result's length on success. NOT NULL TERMINATED ON INPUT OR OUTPUT!!
 */
int Utf16ToCodepoints(uint16_t* utf16, uint32_t* codepoints, int in_length, int* out_length) {
    int in = 0;
    int out = 0;

    while (in < in_length) {
        if (out == *out_length) {
            return ENAMETOOLONG;
        }

        uint32_t codepoint = utf16[in++];
        if (codepoint >= 0xD800 && codepoint <= 0xDBFF) {
            if (in == in_length) {
                return EINVAL;
            }
            uint16_t low_surrogate = utf16[in++];
            if (low_surrogate >= 0xDC00 && low_surrogate <= 0xDFFF) {
                codepoint = (codepoint - 0xD800) * 0x400 + (low_surrogate - 0xDC00);
            } else {
                return EINVAL;
            }

        } else if (codepoint >= 0xDC00 && codepoint <= 0xDFFF) {
            return EINVAL;
        }

        if (codepoint >= 0xD800 && codepoint <= 0xDFFF) {
            return EINVAL;
        }
        
        codepoints[out++] = codepoint;
    }

    *out_length = out;
    return 0;
}

int Utf8ToCodepoints(uint8_t* utf8, uint32_t* codepoints, int in_length, int* out_length) {
    int in = 0;
    int out = 0;

    while (in < in_length) {
        if (out == *out_length) {
            return ENAMETOOLONG;
        }

        uint32_t codepoint = utf8[in++];
        if ((codepoint >> 3) == 0x1E) {
            if (in + 2 >= in_length) {
                return EINVAL;
            }
            codepoint &= 0x7;
            codepoint <<= 6;
            uint8_t next = utf8[in++];
            if ((next >> 6) != 0x2) {
                return EINVAL;
            }
            codepoint |= next & 0x3F;
            codepoint <<= 6;
            next = utf8[in++];
            if ((next >> 6) != 0x2) {
                return EINVAL;
            }
            codepoint |= next & 0x3F;
            next = utf8[in++];
            if ((next >> 6) != 0x2) {
                return EINVAL;
            }
            codepoint |= next & 0x3F;

        } else if ((codepoint >> 4) == 0xE) {
            if (in + 1 >= in_length) {
                return EINVAL;
            }
            codepoint &= 0xF;
            codepoint <<= 6;
            uint8_t next = utf8[in++];
            if ((next >> 6) != 0x2) {
                return EINVAL;
            }
            codepoint |= next & 0x3F;
            codepoint <<= 6;
            next = utf8[in++];
            if ((next >> 6) != 0x2) {
                return EINVAL;
            }
            codepoint |= next & 0x3F;
  
        } else if ((codepoint >> 5) == 0x6) {
            if (in >= in_length) {
                return EINVAL;
            }
            codepoint &= 0x1F;
            codepoint <<= 6;
            uint8_t next = utf8[in++];
            if ((next >> 6) != 0x2) {
                return EINVAL;
            }
            codepoint |= next & 0x3F;

            
        } else if (codepoint >= 0x80) {
            return EINVAL;
        }

        if (codepoint >= 0xD800 && codepoint <= 0xDFFF) {
            return EINVAL;
        }
        
        codepoints[out++] = codepoint;
    }

    *out_length = out;
    return 0;
}

int CodepointsToUtf16(uint32_t* codepoints, uint16_t* utf16, int in_length, int* out_length) {
    int in = 0;
    int out = 0;
    while (in < in_length) {
        if (out == *out_length) {
            return ENAMETOOLONG;
        }

        uint32_t codepoint = codepoints[in++];

        if (codepoint >= 0xD800 && codepoint <= 0xDFFF) {
            return EINVAL;
        }
        if (codepoint > 0x10FFFF) {
            return EINVAL;
        }

        if (codepoint >= 0x10000) {
            uint16_t high_surrogate = (codepoint / 0x400) + 0xD800;
            uint16_t low_surrogate = (codepoint % 0x400) + 0xDC00;
            utf16[out++] = high_surrogate;
            if (out == *out_length) {
                return ENAMETOOLONG;
            }
            utf16[out++] = low_surrogate;

        } else {
            utf16[out++] = codepoint;
        }
    }

    *out_length = out;
    return 0;
}

int CodepointsToUtf8(uint32_t* codepoints, uint8_t* utf8, int in_length, int* out_length) {
    int in = 0;
    int out = 0;
    while (in < in_length) {
        if (out == *out_length) {
            return ENAMETOOLONG;
        }

        uint32_t codepoint = codepoints[in++];

        if (codepoint >= 0xD800 && codepoint <= 0xDFFF) {
            return EINVAL;
        }

        if (codepoint <= 0x7F) {
            utf8[out++] = codepoint;

        } else if (codepoint <= 0x7FF) {
            if (out + 2 > *out_length) {
                return ENAMETOOLONG;
            }
            utf8[out++] = 0xC0 | (codepoint >> 6);
            utf8[out++] = 0x80 | (codepoint & 0x3F);

        } else if (codepoint <= 0xFFFF) {
            if (out + 3 > *out_length) { 
                return ENAMETOOLONG;
            }
            utf8[out++] = 0xE0 | (codepoint >> 12);
            utf8[out++] = 0x80 | ((codepoint >> 6) & 0x3F);
            utf8[out++] = 0x80 | (codepoint & 0x3F);

        } else if (codepoint <= 0x10FFF) {
            if (out + 4 > *out_length) { 
                return ENAMETOOLONG;
            }
            utf8[out++] = 0xF0 | (codepoint >> 18);
            utf8[out++] = 0x80 | ((codepoint >> 12) & 0x3F);
            utf8[out++] = 0x80 | ((codepoint >> 6) & 0x3F);
            utf8[out++] = 0x80 | (codepoint & 0x3F);
        } else {
            return EINVAL;
        }
    }

    *out_length = out;
    return 0;
}


