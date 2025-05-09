#include <stdlib.h>
#include <string.h>

#include "../lib/row.h"

void free_row(Row* row) {
    if (row == NULL) return;
    if (row->chars != NULL) free(row->chars);
    if (row->hl != NULL) free(row->hl);
}

void row_insert_char(Row* row, int at, int c)  {
    if (at < 0 || at > row->size) at = row->size;
    row->chars = (char*) realloc(row->chars, row->size + 2);
    memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
    row->size++;
    row->chars[at] = c;
}

void row_delete_char(Row *row, int at) {
    if (at < 0 || at >= row->size) return;
    memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
    row->size--;
}

void row_append_string(Row *row, char *s, size_t len) {
    row->chars = (char*) realloc(row->chars, row->size + len + 1);
    memcpy(&row->chars[row->size], s, len);
    row->size += len;
    row->chars[row->size] = '\0';
}