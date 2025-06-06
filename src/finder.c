#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#include "../lib/finder.h"

void init_finder() {
    F.indexes = (Index*) malloc(sizeof(Index));
    F.found = 0;
}

Finder *new_finder() {
    Finder *finder = (Finder*) malloc(sizeof(Finder));
    finder->found = 0;
    finder->indexes = (Index*) malloc(sizeof(Index));
    return finder;
}

void free_finder(Finder *finder) {
    free(finder->indexes);
    free(finder);
}

void find_in_row(Finder *finder, size_t row, char *s, char *target) {
    if (s == NULL || target == NULL || strlen(s) == 0 || strlen(target) == 0) {
        return;
    }
    if (!finder->indexes) {
        finder->indexes = (Index*) malloc(sizeof(Index));
    }
    size_t len = strlen(target);
    char *ptr = strstr(s, target);
    while (ptr != NULL) {
        Index index = {row, ptr - s, len};
        memcpy(&finder->indexes[finder->found++], &index, sizeof(Index));
        finder->indexes = (Index*) realloc(finder->indexes, sizeof(Index) * (finder->found + 1));
        ptr = strstr(ptr + len, target);
    }
}

void clear_finder(Finder *finder) {
    if (finder->indexes != NULL) {
        free(finder->indexes);
        finder->indexes = (Index*) malloc(sizeof(Index));
    }
    finder->found = 0;
}