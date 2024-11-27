#ifndef syntax_h
#define syntax_h

#include <stddef.h>

typedef struct ColoredRow {
    int *colors;
    size_t size;
} ColoredRow;

typedef struct Colored {
    ColoredRow *rows;
    size_t size;
} Colored;

#endif