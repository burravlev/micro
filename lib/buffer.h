#ifndef BUFFER_H
#define BUFFER_H

typedef struct Buffer {
    char *buffer;
    int length;
} Buffer;

#define ABUF_INIT {NULL, 0}

void b_append(Buffer* buffer, const char* s, int length);
void b_clear_screen(Buffer *buf);
void b_hide_cursor(Buffer *buf);
void b_show_cursor(Buffer *buf);
void b_flush(Buffer *buf);
void b_free(Buffer *buf);

#endif