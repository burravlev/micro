#ifndef finder_h
#define finder_h

#include <stddef.h>
#include <string.h>
#include <stdlib.h>

typedef struct Index {
    size_t row;
    size_t i;
    size_t len;
} Index;

typedef struct Finder {
    Index *indexes;
    size_t found;
} Finder;

struct Finder F;

void init_finder();
Finder *new_finder();
void free_finder(Finder *finder);
void find_in_row(Finder *finder, size_t row, char *s, char *target);
void clear_finder(Finder *finder);

#endif