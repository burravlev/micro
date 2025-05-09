#ifndef ROW_H
#define ROW_H

#include <stddef.h>
#include <stdlib.h>

typedef struct Row {
    char* chars;
    int size;
    unsigned char* hl;
} Row;

void free_row(Row* row);
void row_insert_char(Row* row, int at, int c);
void row_delete_char(Row *row, int at);
void row_append_string(Row *row, char *s, size_t len);

#endif