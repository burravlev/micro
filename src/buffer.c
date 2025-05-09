#include <stdlib.h>
#include <strings.h>

#include "../lib/buffer.h"
#include "../lib/terminal.h"

void b_append(Buffer *buf, const char *s, int length) {
    if (buf == NULL) return;
    char *new_buf = (char*) realloc(buf->buffer, buf->length + length);

    if (new_buf == NULL) return;
    memcpy(&new_buf[buf->length], s, length);
    buf->buffer = new_buf;
    buf->length += length;
}

void b_free(Buffer *buf) {
    if (buf != NULL && buf->buffer != NULL) {
        free(buf->buffer);
    }
}

void b_clear_screen(Buffer *buf) {
    b_append(buf, CLEAR_SCREEN, strlen(CLEAR_SCREEN));
}

void b_hide_cursor(Buffer *buf) {
    b_append(buf, HIDE_CURSOR, strlen(HIDE_CURSOR));
}

void b_show_cursor(Buffer *buf) {
    b_append(buf, SHOW_CURSOR, strlen(SHOW_CURSOR));
}

void b_flush(Buffer *buf) {
    term_write(buf->buffer, buf->length);
    b_free(buf);
}
