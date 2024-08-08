#define VERSION "0.0.1"
#define TAB_STOP 8
#define QUIT_TIMES 1

#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "finder.h"

#define CTRL_KEY(k) ((k) & 0x1f)

typedef struct editor_row {
    int size;
    int rsize;
    char *chars;
    char *render;
} editor_row;

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

typedef struct editor {
    int cx, cy;
    int rx;
    int rowoff;
    int coloff;
    int screenrows;
    int screencols;
    int numrows;
    editor_row *rows;
    int dirty;
    char *filename;
    char statusmsg[80];
    time_t statusmsg_time;
    struct termios orig_termios;
} editor;

struct editor E;

struct finder F;

void set_status_message(const char *fmt, ...);
void refresh_screen();
char *eprompt(char *prompt, void (*callback)(char *, int));

void die(const char *s) {
    write(STDIN_FILENO, "\x1b[2J", 4);
    write(STDIN_FILENO, "\x1b[H", 3);

    perror(s);
    exit(1);
}

void disable_raw_mode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1) {
        die("tcsetattr");
    }
}

void enable_raw_mode() {
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) die("tcgetattr");
    atexit(disable_raw_mode);

    struct termios raw = E.orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw)) die("tcsetattr");
}

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

int get_cursor_position(int *rows, int *cols) {
    char buf[32];
    unsigned int i = 0;

    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

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

int get_window_size(int *rows, int *cols) {
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 1 || ws.ws_col == 0) {
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
        return get_cursor_position(rows, cols);
    } else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

int row_cx_to_rx(editor_row *row, int cx) {
    int rx = 0;
    int j;
    for (j = 0; j < cx; j++) {
        if (row->chars[j] == '\t') {
            rx += (TAB_STOP - 1) - (rx % TAB_STOP);
        }
        rx++;
    }
    return rx;
}

int row_rx_to_cx(editor_row *row, int rx) {
    int cur_rx = 0;
    int cx;
    for (cx = 0; cx <row->size; cx++) {
        if (row->chars[cx] == '\t') {
            cur_rx += (TAB_STOP - 1) - (cur_rx % TAB_STOP);
        }
        cur_rx++;

        if (cur_rx > rx) return cx;
    }
    return cx;
}

void update_row(editor_row *row) {
    int tabs = 0;
    int j;
    for (j = 0; j < row->size; j++)
        if (row->chars[j] == '\t') tabs++;

    free(row->render);
    row->render = malloc(row->size + tabs*(TAB_STOP - 1) + 1);

    int idx = 0;
    for (j = 0; j < row->size; j++) {
        if (row->chars[j] == '\t') {
            row->render[idx++] = ' ';
            while (idx % TAB_STOP != 0) row->render[idx++] = ' ';
        } else {
            row->render[idx++] = row->chars[j];
        }
    }
    row->render[idx] = '\0';
    row->rsize = idx;
}

void append_row(int at, char *s, size_t len) {
    if (at < 0 || at > E.numrows) return;

    E.rows = realloc(E.rows, sizeof(editor_row) * (E.numrows + 1));
    memmove(&E.rows[at + 1], &E.rows[at], sizeof(editor_row) * (E.numrows - at));

    E.rows[at].size = len;
    E.rows[at].chars = malloc(len + 1);
    memcpy(E.rows[at].chars, s, len);
    E.rows[at].chars[len] = '\0';

    E.rows[at].rsize = 0;
    E.rows[at].render = NULL;
    update_row(&E.rows[at]);

    E.numrows++;
    E.dirty++;
}

void free_row(editor_row *row) {
    free(row->render);
    free(row->chars);
}

void delete_row(int at) {
    if (at < 0 || at >= E.numrows) return;
    free_row(&E.rows[at]);
    memmove(&E.rows[at], &E.rows[at + 1], sizeof(editor_row) * (E.numrows - at - 1));
    E.numrows--;
    E.dirty++;
}

void row_insert_char(editor_row *row, int at, int c) {
    if (at < 0 || at > row->size) at = row->size;
    row->chars = realloc(row->chars, row->size + 2);
    memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
    row->size++;
    row->chars[at] = c;
    update_row(row);
    E.dirty++;
}

void insert_char(int c) {
    if (E.cy == E.numrows) {
        append_row(E.numrows, "", 0);
    }
    row_insert_char(&E.rows[E.cy], E.cx, c);
    E.cx++;
}

void insert_new_line() {
    if (E.cx == 0) {
        append_row(E.cy, "", 0);
    } else {
        editor_row *row = &E.rows[E.cy];
        append_row(E.cy + 1, &row->chars[E.cx], row->size - E.cx);
        row = &E.rows[E.cy];
        row->size = E.cx;
        row->chars[row->size] = '\0';
        update_row(row);
    }
    E.cy++;
    E.cx = 0;
}

void row_delete_char(editor_row *row, int at) {
    if (at < 0 || at >= row->size) return;
    memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
    row->size--;
    update_row(row);
    E.dirty++;
}

void row_append_string(editor_row *row, char *s, size_t len) {
    row->chars = realloc(row->chars, row->size + len + 1);
    memcpy(&row->chars[row->size], s, len);
    row->size += len;
    row->chars[row->size] = '\0';
    update_row(row);
    E.dirty++;
}

void delete_char() {
    if (E.cy == E.numrows) return;  
    if (E.cx == 0 && E.cy == 0) return;

    editor_row *row = &E.rows[E.cy];
    if (E.cx > 0) {
        row_delete_char(row, E.cx - 1);
        E.cx--;
    } else {
        E.cx = E.rows[E.cy - 1].size;
        row_append_string(&E.rows[E.cy - 1], row->chars, row->size);
        delete_row(E.cy);
        E.cy--;
    }
}

char *row_to_string(int *buflen) {
    int totlen = 0;
    int j;
    for (j = 0; j < E.numrows; j++) {
        totlen += E.rows[j].size + 1;
    }
    *buflen = totlen;

    char *buf = malloc(totlen);
    char *p = buf;
    for (j = 0; j < E.numrows; j++) {
        memcpy(p, E.rows[j].chars, E.rows[j].size);
        p += E.rows[j].size;
        *p = '\n';
        p++;
    }

    return buf;
}

void eopen(char *filename) {
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
        append_row(E.numrows, line, linelen);
    }
    free(line);
    fclose(fp);
    E.dirty = 0;
}

void find_callback(char *query, int key) {
    static int last_match = -1;
    static int direction = 1;

    if (key == '\r' || key == '\x1b') {
        last_match = -1;
        direction = 1;
        return;
    } else if (key == ARROW_RIGHT || key == ARROW_DOWN) {
        direction = 1;
    } else if (key == ARROW_LEFT || key == ARROW_UP) {
        direction = -1;
    } else {
        last_match = -1;
        direction = 1;
    }

    if (last_match == -1) direction = 1;
    int current = last_match;

    int i;
    for (i = 0; i < E.numrows; i++) {
        current += direction;
        if (current == -1) current = E.numrows - 1;
        else if (current == E.numrows) current = 0;

        editor_row *row = &E.rows[current];
        char *match = strstr(row->render, query);
        if (match) {
            last_match = current;
            E.cy = current;
            E.cx = row_rx_to_cx(row, match - row->render);
            E.rowoff = E.numrows;
            break;
        }
    }
}

void efind_callback(char *query, int key) {
    if (key == '\r' || key == '\x1b') {
        fclean(&F);
    } else if (key == ARROW_RIGHT || key == ARROW_DOWN) {
        fnext(&F, &E.cx, &E.cy);
    } else if (key == ARROW_LEFT || key == ARROW_UP) {
        fprev(&F, &E.cx, &E.cy);
    } else {
        fclean(&F);
        for (int i = 0; i < E.numrows; i++) {
            ffind(&F, E.rows[i].chars, query, i);
        }
        fnext(&F, &E.cx, &E.cy);
    }
}

int *efind_in_row(char *s, char *query, int *n) {
    int index = 0;
    char *ptr = s;
    int count = 0;
    int l = strlen(query);

    int *indexes = malloc(sizeof(int));
    int updated = 0;

    while ((ptr = strstr(ptr, query)) != NULL) {
        updated++;
        indexes = realloc(indexes, sizeof(int) * (count + 1));
        indexes[count] = ptr - s;
        count++;
        ptr += l - 1;
    }

    if (!updated) {
        return NULL;
    }
    *n = count;
    return indexes;
}

void efind() {
    int saved_cx = E.cx;
    int saved_cy = E.cy;
    int saved_coloff = E.coloff;
    int saved_rowoff = E.rowoff;

    char *query = eprompt("Search: %s (Use ESC/Arrows/Enter)", efind_callback);

    if (query) {
        free(query);
    } else {
        E.cx = saved_cx;
        E.cy = saved_cy;
        E.coloff = saved_coloff;
        E.rowoff = saved_rowoff;
    }
}

void esave() {
    if (E.filename == NULL) {
        E.filename = eprompt("Save as: %s (ESC to cancel)", NULL);
        if (E.filename == NULL) {
            set_status_message("Save aborted");
            return;
        }
    }

    int len;
    char *buf = row_to_string(&len);

    int fd = open(E.filename, O_RDWR | O_CREAT, 0644);
    if (fd != -1) {
        if (ftruncate(fd, len) != -1) {
            if (write(fd, buf, len) == len) {
                close(fd);
                free(buf);
                E.dirty = 0;
                set_status_message("%d bytes written to disk", len);
                return;
            }
        }
        close(fd);
    }
    free(buf);
    set_status_message("Can't save! I/O error: %s", strerror(errno));
}

typedef struct abuf {
    char *buf;
    int len;
} abuf;

#define ABUF_INIT {NULL, 0}

void ab_append(struct abuf *ab, const char *s, int len) {
    char *new = realloc(ab->buf, ab->len + len);

    if (new == NULL) return;
    memcpy(&new[ab->len], s, len);
    ab->buf = new;
    ab->len += len;
}

void ab_free(struct abuf *ab) {
    free(ab->buf);
}

void scroll() {
    E.rx = 0;
    if (E.cy < E.numrows) {
        E.rx = row_cx_to_rx(&E.rows[E.cy], E.cx);
    }

    if (E.cy < E.rowoff) {
        E.rowoff = E.cy;
    }
    if (E.cy >= E.rowoff + E.screenrows) {
        E.rowoff = E.cy - E.screenrows + 1;
    }
    if (E.rx < E.coloff) {
        E.coloff = E.rx;
    }
    if (E.rx >= E.coloff + E.screencols) {
        E.coloff = E.rx - E.screencols + 1;
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
            "MICRO editor -- version %s", VERSION);
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
            int len = E.rows[filerow].rsize - E.coloff;
            if (len < 0) len = 0;
            if (len > E.screencols) len = E.screencols;
            ab_append(ab, &E.rows[filerow].render[E.coloff], len);
        }
        ab_append(ab, "\x1b[K", 3);
        ab_append(ab, "\r\n", 2);
    }
}

void draw_status_bar(struct abuf *ab) {
    ab_append(ab, "\x1b[7m", 4);
    char status[80], rstatus[80];
    int len = snprintf(status, sizeof(status), "%.20s - %d lines %s",
        E.filename ? E.filename : "[No Name]", E.numrows,
        E.dirty ? "(modified)" : "");
    int rlen = snprintf(rstatus, sizeof(rstatus), "%d:%d",
        E.cy + 1, E.numrows);
    if (len > E.screencols) len = E.screencols;
    ab_append(ab, status, len);
    while (len < E.screencols) {
        if (E.screencols - len == rlen) {
            ab_append(ab, rstatus, rlen);
            break;
        } else {
            ab_append(ab, " ", 1);
            len++;
        }
    }
    ab_append(ab, "\x1b[m", 3);
    ab_append(ab, "\r\n", 2);
}

void draw_message_bar(struct abuf *ab) {
    ab_append(ab, "\x1b[K", 3);
    int msglen = strlen(E.statusmsg);
    if (msglen > E.screencols) msglen = E.screencols;
    if (msglen && time(NULL) - E.statusmsg_time < 5) {
        ab_append(ab, E.statusmsg, msglen);
    }
}

void set_status_message(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
    va_end(ap);
    E.statusmsg_time = time(NULL);
}

void refresh_screen() {
    scroll();

    struct abuf ab = ABUF_INIT;
    
    ab_append(&ab, "\x1b[?25l", 6);
    ab_append(&ab, "\x1b[H", 3);

    draw_rows(&ab);
    draw_status_bar(&ab);
    draw_message_bar(&ab);

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1,
                                              (E.rx - E.coloff) + 1);
    ab_append(&ab, buf, strlen(buf));

    ab_append(&ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO, ab.buf, ab.len);
    ab_free(&ab);
}

char *eprompt(char *prompt, void (*callback)(char*, int)) {
    size_t bufsize = 128;
    char *buf = malloc(bufsize);

    size_t buflen = 0;
    buf[0] = '\0';

    while (1) {
        set_status_message(prompt, buf);
        refresh_screen();

        int c = read_key();
        if (c == DEL_KEY || c == CTRL_KEY('h') || c == BACKSPACE) {
            if (buflen != 0) buf[--buflen] = '\0';
        } else if (c == '\x1b') {
            set_status_message("");
            if (callback) callback(buf, c);
            free(buf);
            return NULL;
        } else if (c == '\r') {
            if (buflen != 0) {
                set_status_message("");
                if (callback) callback(buf, c);
                return buf;
            }
        } else if (!iscntrl(c) && c < 128) {
            if (buflen == bufsize - 1) {
                bufsize += 2;
                buf = realloc(buf, bufsize);
            }
            buf[buflen++] = c;
            buf[buflen] = '\0';
        }
        if (callback) callback(buf, c);
    }
}

void move_cursor(int key) {
    editor_row *row = (E.cy >= E.numrows) ? NULL : &E.rows[E.cy];

    switch (key) {
        case ARROW_LEFT:
        if (E.cx != 0) {
            E.cx--;
        } else if (E.cy > 0) {
            E.cy--;
            E.cx = E.rows[E.cy].size;
        }
        break;
        case ARROW_RIGHT:
        if (row && E.cx < row->size) {
            E.cx++;
        } else if (row && E.cx == row->size) {
            E.cy++;
            E.cx = 0;
        }
        break;
        case ARROW_UP:
        if (E.cy != 0) {
            E.cy--;
        }
        break;
        case ARROW_DOWN:
        if (E.cy < E.numrows) {
            E.cy++;
        }
        break;
    }

    row = (E.cy >= E.numrows) ? NULL : &E.rows[E.cy];
    int rowlen = row ? row->size : 0;
    if (E.cx > rowlen) {
        E.cx = rowlen;
    }
}

void process_keypress() {
    static int quit_times = QUIT_TIMES;

    int c = read_key();

    switch (c) {
        case '\r':
        insert_new_line();
        break;

        case CTRL_KEY('q'):
        if (E.dirty && quit_times > 0) {
            set_status_message("File has unsaved changes! Press Ctrl-Q one more time to quit.", 
                quit_times);
            quit_times--;
            return;
        }
        write(STDIN_FILENO, "\x1b[2J", 4);
        write(STDIN_FILENO, "\x1b[H", 3);
        exit(0);
        break;

        case CTRL_KEY('s'):
        esave();
        break;
        
        case HOME_KEY:
        E.cx = 0;
        break;

        case CTRL_KEY('f'):
        efind();
        break;

        case END_KEY:
        if (E.cy < E.numrows) {
            E.cx = E.rows[E.cy].size;
        }
        break;

        case BACKSPACE:
        case CTRL_KEY('h'):
        case DEL_KEY:
        if (c == DEL_KEY) move_cursor(ARROW_RIGHT);
        delete_char();
        break;

        case PAGE_UP:
        case PAGE_DOWN:
        {
            if (c == PAGE_UP) {
                E.cy = E.rowoff;
            } else if (c == PAGE_DOWN) {
                E.cy = E.rowoff + E.screenrows - 1;
                if (E.cy > E.numrows) E.cy = E.numrows;
            }
        }
        break;

        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
        move_cursor(c);
        break;

        case CTRL_KEY('l'):
        case '\x1b':

        break;

        default:
            insert_char(c);
            break;
    }
    quit_times = QUIT_TIMES;
}

void init_editor() {
    E.cx = 0;
    E.cy = 0;
    E.rx = 0;
    E.rowoff = 0;
    E.coloff = 0;
    E.numrows = 0;
    E.rows = NULL;
    E.dirty = 0;
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
        eopen(argv[1]);
    }

    set_status_message("HELP: Ctrl-S = save | Ctrl-Q to quit | Ctrl-F to find");

    while (1) {
        refresh_screen();
        process_keypress();
    }
    return 0;
}