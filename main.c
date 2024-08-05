#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#define MICRO_VERSION "0.0.1"
#define MICRO_TAB_STOP 8
#define CTRL_KEY(k) ((k) & 0x1f)

#define ABUF_INIT {NULL, 0}

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

// Utility
int int_len(int n) {
    if (n >= 1000000000) return 10;
    if (n >= 100000000)  return 9;
    if (n >= 10000000)   return 8;
    if (n >= 1000000)    return 7;
    if (n >= 100000)     return 6;
    if (n >= 10000)      return 5;
    if (n >= 1000)       return 4;
    if (n >= 100)        return 3;
    if (n >= 10)         return 2;
    return 1;
}

enum editor_key {
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

// data
char buf[32];

struct erow {
    int size;
    int rsize;
    char *chars;
    char *render;
} typedef erow;

struct editor_config {
    int cx, cy;
    char *filename;
    int rowoff;
    int coloff;
    int screenrows;
    int screencols;
    int numrows;
    erow *row;
    char statusmsg[80];
    time_t statusmsg_time;
    struct termios orig_termios;
};

struct editor_config E;

// terminal
void die(const char *s) {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    perror(s);
    exit(1);
}

void disable_raw_mode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios)) {
        die("tcsetattr");
    }
}

void enable_raw_mode() {
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) {
        die("tcgetattr");
    }
    atexit(disable_raw_mode);

    struct termios raw = E.orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

char read_key() {
    int nread;
    char c;
    while ((nread == read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) die("read");
    }
    return c;
}

int get_cursor_position(int *rows, int *cols) {
    char buf[32];
    unsigned int i = 0;

    if (write(STDERR_FILENO, "\x1b[6n", 4) != 4) return -1;

    while (i < sizeof(buf) - 1) {
        if (read(STDERR_FILENO, &buf[i], 1) != 1) break;
        if (buf[i] == 'R') break;
        i++;
    }
    buf[i] = '\0';

    if (buf[0] != '\x1b' || buf[1] != '[') return -1;
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;

    return -1;
}

int get_window_size(int *rows, int *cols) {
    struct winsize ws;

    if (ioctl(STDERR_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        if (write(STDERR_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
        return get_cursor_position(rows, cols);
    } else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

int row_cx_to_rx(erow *row, int cx) {
    int rx = 0;
    int j;
    for (j = 0; j < cx; j++) {
        if (row->chars[j] == '\t')
            rx += (MICRO_TAB_STOP - 1) - (rx % MICRO_TAB_STOP);
        rx++;
    }
    return rx;
}

void update_row(erow *row) {
    int tabs = 0;
    int j;
    for (j = 0; j < row->size; j++)
        if (row->chars[j] == '\t') tabs++;

    free(row->render);
    row->render = malloc(row->size + tabs*(MICRO_TAB_STOP - 1) + 1);

    row->render = malloc(row->size + tabs*7 + 1);
    int idx = 0;
    for (j = 0; j < row->size; j++) {
        if (row->chars[j] == '\t') {
            row->render[idx++] = ' ';
            while (idx % MICRO_TAB_STOP != 0) row->render[idx++] = ' ';
        } else {
            row->render[idx++] = row->chars[j];
        }
    }
    row->render[idx] = '\0';
    row->rsize = idx;
}

void einsert_row(int at, char *s, size_t len) {
    if (at < 0 || at > E.numrows) return;

    E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));
    memmove(&E.row[at + 1], &E.row[at], sizeof(erow) * (E.numrows - at));

    E.row[at].size = len;
    E.row[at].chars = malloc(len + 1);
    memcpy(E.row[at].chars, s, len);
    E.row[at].chars[len] = '\0';

    E.row[at].rsize = 0;
    E.row[at].render = NULL;
    update_row(&E.row[at]);

    E.numrows++;
}

void efree_row(erow *row) {
    free(row->render);
    free(row->chars);
}

void edelete_row(int at) {
    if (at < 0 || at >= E.numrows) return;
    efree_row(&E.row[at]);
    memmove(&E.row[at], &E.row[at + 1], sizeof(erow) * (E.numrows - at - 1));
    E.numrows--;
}

void erow_insert_char(erow *row, int at, int c) {
    if (at < 0 || at > row->size) at = row->size;
    row->chars = realloc(row->chars, row->size + 2);
    memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
    row->size++;
    row->chars[at] = c;
    update_row(row);
}

void row_delete_char(erow *row, int at) {
    if (at < 0 || at >= row->size) return;
    memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
    row->size--;
    update_row(row);
}

void einsert_char(int c) {
    if (E.cy == E.numrows) {
        einsert_row(E.numrows, "", 0);
    }
    erow_insert_char(&E.row[E.cy], E.cx, c);
    E.cx++;
}

void einsert_newline() {
    if (E.cx == 0) {
        einsert_row(E.cy, "", 0);
    } else {
        erow *row = &E.row[E.cy];
        einsert_row(E.cy + 1, &row->chars[E.cx], row->size - E.cx);
        row = &E.row[E.cy];
        row->size = E.cx;
        row->chars[row->size] = '\0';
        update_row(row);
    }
    E.cy++;
    E.cx = 0;
}

void erow_append_string(erow *row, char *s, size_t len) {
    row->chars = realloc(row->chars, row->size + len + 1);
    memcpy(&row->chars[row->size], s, len);
    row->size += len;
    row->chars[row->size] = '\0';
    update_row(row);
}

void delete_char() {
    if (E.cx == 0 && E.cy == 0) return;

    erow *row = &E.row[E.cy];
    if (E.cx > 0) {
        row_delete_char(row, E.cx - 1);
        E.cx--;
    } else {
        E.cx = E.row[E.cy - 1].size;
        erow_append_string(&E.row[E.cy - 1], row->chars, row->size);
        edelete_row(E.cy);
        E.cy--;
    }
}

void open_file(char *filename) {
    free(E.filename);
    E.filename = strdup(filename);

    FILE *fp = fopen(filename, "r");
    if (!fp) die("fopen");

    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;
    while ((linelen = getline(&line, &linecap, fp)) != -1) {
        while (linelen > 0 && (line[linelen - 1] == '\n' ||
                            line[linelen - 1] == '\r'))
        linelen--;
        einsert_row(E.numrows, line, linelen);
    }
    free(line);
    fclose(fp);
}

struct abuf {
    char *b;
    int len;
};

void ab_append(struct abuf *ab, const char *s, int len) {
    char *new = realloc(ab->b, ab->len + len);

    if (new == NULL) return;
    memcpy(new+ab->len, s, len);
    ab->b = new;
    ab->len += len;
}

void ab_free(struct abuf *ab) {
    free(ab->b);
}

void scroll_screen() {
    if (E.cy < E.rowoff) {
        E.rowoff = E.cy;
    }
    if (E.cy >= E.rowoff + E.screenrows) {
        E.rowoff = E.cy - E.screenrows + 1;
    }
    if (E.cx < E.coloff) {
        E.coloff = E.cx;
    }
    if (E.cx >= E.coloff + E.screencols) {
        E.coloff = E.cy - E.screencols + 1;
    }
}

void draw_rows(struct abuf *ab) {
    int y;
    for (y = 0; y < E.screenrows; y++) {
        int filerow = y + E.rowoff;
        if (filerow >= E.numrows) {
            if (E.numrows == 0 && y == E.screenrows / 3) {
                char welcome[80];
                int welcomelen = snprintf(welcome, sizeof(welcome),
                "Micro editor -- version %s", MICRO_VERSION);
                if (welcomelen > E.screencols) welcomelen = E.screencols;
                int padding = (E.screencols - welcomelen) / 2;
                if (padding) {
                ab_append(ab, "~", 1);
                padding--;
                }
                while (padding--) ab_append(ab, " ", 1);
                ab_append(ab, welcome, welcomelen);
            } else {
                ab_append(ab, "~", 1);
            }
        } else {
            int len = E.row[filerow].rsize - E.coloff;
            if (len < 0) len = 0;
            if (len > E.screencols) len = E.screencols;
            ab_append(ab, &E.row[filerow].render[E.coloff], len);
        }
        ab_append(ab, "\x1b[K", 3);
        ab_append(ab, "\r\n", 2);
    }
}

void draw_status_bar(struct abuf *ab) {
    ab_append(ab, "\x1b[7m", 4);
    char *status = (char*) malloc(E.screencols);
    char *filename = E.filename ? E.filename : "[No Name]";

    size_t i;
    for (i = 0; i < strlen(filename); i++) {
        if (i < 20) {
            status[i] = filename[i];
        } else {
            break;
        }
    }
    if (i < E.screencols) {
        status[i++] = ' ';
    }
    int cols = int_len(E.cx) + int_len(E.cy) + 1;
    for (; i < E.screencols - cols; i++) {
        status[i] = ' ';
    }
    char buf[cols+1];
    snprintf(buf, sizeof(buf), "%d:%d", E.cx, E.cy);
    for (int j = 0; j <= cols; j++) {
        status[i++] = buf[j];
    }
    ab_append(ab, status, strlen(status));
    ab_append(ab, "\x1b[m", 3);
    ab_append(ab, "\r\n", 2);
}

void draw_message_bar(struct abuf *ab) {
    ab_append(ab, "\x1b[K", 3);
    size_t msglen = strlen(E.statusmsg);
    if (msglen > E.screencols) msglen = E.screencols;
    ab_append(ab, E.statusmsg, msglen);
}

void refresh_screen() {
    scroll_screen();

    struct abuf ab = ABUF_INIT;

    ab_append(&ab, "\x1b[?25l", 6);
    ab_append(&ab, "\x1b[H", 3);

    draw_rows(&ab);
    draw_status_bar(&ab);
    draw_message_bar(&ab);

    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1, 
                                              (E.cx - E.coloff) + 1);
    
    ab_append(&ab, buf, strlen(buf));
    ab_append(&ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO, ab.b, ab.len);
    ab_free(&ab);
}

void set_status_msg(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
    va_end(ap);
    E.statusmsg_time = time(NULL);
}

void move_cursor(char key) {
    erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];

    switch (key) {
    case 'a':
        if (E.cx != 0) {
            E.cx--;
        } else if (E.cy > 0) {
            E.cy--;
            E.cx = E.row[E.cy].size;
        }
        break;
    case 'd':
        if (row && E.cx < row->size) {
            E.cx++;
        } else if (row && E.cx == row->size) {
            E.cy++;
            E.cx = 0;
        }
        break;
    case 'w':
        if (E.cy != 0) {
            E.cy--;
        }
        break;
    case 's':
        if (E.cy < E.numrows) {
            E.cy++;
        }
        break;
    }

    row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
    int rowlen = row ? row->size : 0;
    if (E.cx > rowlen) {
        E.cx = rowlen;
    }
}

void press_key() {
    char c = read_key();
    switch (c) {
        case CTRL_KEY('q'):
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;
        case '\r':
            einsert_newline();
            break;
        case 'w':
        case 's':
        case 'a':
        case 'd':
            move_cursor(c);
            break;
        case BACKSPACE:
        case CTRL_KEY('h'):
        case DEL_KEY:
            if (c == DEL_KEY) {
                move_cursor('d');
            }
            delete_char();
            break;
        default:
            einsert_char(c);
            break;
    }
}

// init
void init_editor() {
    E.cx = 0;
    E.cy = 0;
    E.rowoff = 0;
    E.coloff = 0;
    E.numrows = 0;
    E.row = NULL;
    E.filename = NULL;
    E.statusmsg[0] = '\0';
    E.statusmsg_time = 0;

    if (get_window_size(&E.screenrows, &E.screencols) == -1) die("get_window_size");
    E.screenrows -= 2;
}

int main(int argc, char *argv[]) {
    enable_raw_mode();
    init_editor();
    if (argc >= 2) {
        open_file(argv[1]);
    }

    set_status_msg("HELP: Ctrl-Q = quit");

    while (1) {
        refresh_screen();
        press_key();
    }
    return 0;
}