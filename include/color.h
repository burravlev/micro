#ifndef COLOR_H
#define COLOR_H

#include <stddef.h>
#include <string.h>

int color_to_ansi(size_t r, size_t g, size_t b) {
    if (r < 0) r = 0; if (r > 255) r = 255;
    if (g < 0) g = 0; if (g > 255) g = 255;
    if (b < 0) b = 0; if (b > 255) b = 255;

    if (r == g && g == b) {
        return 232 + (r * 23) / 255;
    } else {
        return 16 + (36 * (r / 51)) + (6 * (g / 51)) + (b / 51);
    }
}

char* color_to_string(size_t r, size_t g, size_t b) {
    int color = color_to_ansi(r, g, b);
    char buf[256];
    snprintf(buf, 256, "\x1b[48;5;%dm", color);
    return buf;
}

#endif