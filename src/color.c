#include "../lib/color.h"

int color_to_ansi(size_t r, size_t g, size_t b){
    if (r < 0) r = 0; if (r > 255) r = 255;
    if (g < 0) g = 0; if (g > 255) g = 255;
    if (b < 0) b = 0; if (b > 255) b = 255;

    if (r == g && g == b) {
        return 232 + (r * 23) / 255;
    } else {
        return 16 + (36 * (r / 51)) + (6 * (g / 51)) + (b / 51);
    }
}