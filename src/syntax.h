#ifndef syntax_h
#define syntax_h

#include <stddef.h>
#include "finder.h"
#include <string.h>
#include <stdlib.h>

enum Color {
    DEFAULT = 0,
    YELLOW = 32,
    RED = 33,
    YELLOW_BG = 43,
};

typedef struct ColoredRow {
    int *colors;
    size_t size;
} ColoredRow;

typedef struct Colored {
    ColoredRow *rows;
    size_t size;
} Colored;

struct Colored C;

void init_colors() {
    C.rows = NULL;
    C.size = 0;
}

void color(Colored *color, char *s, size_t rows) {
    ColoredRow *colored = (ColoredRow*) malloc(sizeof(ColoredRow) * rows);
    for (size_t i = 0; i < rows; i++) {
        ColoredRow row;
        row.size = strlen(s);
        row.colors = (int*) malloc(row.size * sizeof(int));
        for (size_t j = 0; j < row.size; j++) {
            row.colors[j] = DEFAULT;
        }
        colored[i] = row;
    }
    if (color->rows != NULL) {
        free(color->rows);
    }
    color->rows = colored;
    color->size = rows;
}

void color_by_indexes(Colored *color, Index *indexes, size_t count) {
    for (size_t i = 0; i < count; i++) {
        Index index = indexes[i];
        ColoredRow row = color->rows[index.row];
        for (size_t j = index.i; j < index.i + index.len; j++) {
            row.colors[j] = YELLOW_BG;
        }
    }
}

#endif