#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <errno.h>

#define ESC_CHAR '\x1b'
#define STR_END '\0'
#define CLEAR_SCREEN "\x1b[2J"
#define HIDE_CURSOR "\x1b[?25l"
#define SHOW_CURSOR "\x1b[?25h"

#define CTRL_KEY(k) ((k) & 0x1f)

struct termios instance;

void die(const char* s) {
    write(STDIN_FILENO, CLEAR_SCREEN, strlen(CLEAR_SCREEN));
    write(STDIN_FILENO, SHOW_CURSOR, strlen(CLEAR_SCREEN));

    perror(s);
    exit(1);
}

void disable_raw_mode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &instance) == -1) {
        die("tcsetattr");
    }
}

void enable_raw_mode() {
    if (tcgetattr(STDIN_FILENO, &instance) == -1) die("tcgetattr");
    atexit(disable_raw_mode);

    struct termios raw = instance;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw)) {
        die("tcsetattr");
    }
}

typedef struct Buffer {
    char *buffer;
    int length;
} Buffer;

#define ABUF_INIT {NULL, 0}

void b_append(Buffer *buf, const char *s, int length) {
    if (buf == NULL) return;
    char *new_buf = (char*) realloc(buf->buffer, buf->length + length);

    if (new_buf == NULL) return;
    memcpy(&new_buf[buf->length], s, length);
    buf->buffer = new_buf;
    buf->length += length;
}

static void b_free(Buffer *buf) {
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
    write(STDIN_FILENO, buf->buffer, buf->length);
    b_free(buf);
}

#define REQUIRE_CURSOR_POSITION "\x1b[6n"

int get_cursor_position(int *rows, int *cols) {
    char buf[32];
    unsigned int i = 0;

    if (write(STDOUT_FILENO, REQUIRE_CURSOR_POSITION, 4) != 4) return -1;

    while (i < sizeof(buf) - 1) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
        if (buf[i] == 'R') break;
        i++;
    }
    buf[i] = '\0';

    if (buf[0] != '\x1b' || buf[1] != '[') return -1;
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;

    return 0;
}

#define CURSOR_MAX_RIGHT_AND_DOWN "\x1b[999C\x1b[999B"

int get_window_size(int *rows, int *cols) {
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 1 || ws.ws_col == 0) {
        if (write(STDOUT_FILENO, CURSOR_MAX_RIGHT_AND_DOWN, 12) != 12) return -1;
        return get_cursor_position(rows, cols);
    } else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

void clear_screen(void) {
    write(STDOUT_FILENO, CLEAR_SCREEN, strlen(CLEAR_SCREEN));
}

enum EditorKey {
    BACKSPACE = 127,
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    DEL_KEY,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN
};

int read_key() {
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) die("read");
    }

    if (c == '\x1b') {
        char seq[3];

        if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

        if (seq[0] == '[') {
            if (seq[1] >= '0' && seq[1] <= '9') {
                if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
                if (seq[2] == '~') {
                switch (seq[1]) {
                        case '1': return HOME_KEY;
                        case '3': return DEL_KEY;
                        case '4': return END_KEY;
                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;
                        case '7': return HOME_KEY;
                        case '8': return END_KEY;
                    }
                }
            } else {
                switch (seq[1]) {
                    case 'A': return ARROW_UP;
                    case 'B': return ARROW_DOWN;
                    case 'C': return ARROW_RIGHT;
                    case 'D': return ARROW_LEFT;
                    case 'H': return HOME_KEY;
                    case 'F': return END_KEY;
                }
            }
        } else if (seq[0] == 'O') {
            switch (seq[1]) {
                case 'H': return HOME_KEY;
                case 'F': return END_KEY;
            }
        }

        return '\x1b';
    } else {
        return c;
    }
}