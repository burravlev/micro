#define VERSION "0.0.1"
#define TAB_SIZE 4
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

#include "../lib/editor.h"
#include "../lib/color.h"

#define CTRL_KEY(k) ((k) & 0x1f)

struct Editor E;

int is_separator(int c) {
    return isspace(c) || c == '\0' || strchr(",.()+-/*=~%<>[];(){}[]", c) != NULL;
}

void update_syntax(Row *row) {
    row->hl = (unsigned char*) realloc(row->hl, row->size);
    memset(row->hl, HL_NORMAL, row->size);

    if (E.syntax == NULL) return;

    char *scs = E.syntax->singleline_comment_start;
    int scs_len = scs ? strlen(scs) : 0;

    int prev_sep = 1;
    int in_string = 0;

    for (int i = 0; i < row->size; i++) {
        char c = row->chars[i];

        if (scs_len && !in_string) {
            if (!strncmp(&row->chars[i], scs, scs_len)) {
                memset(&row->hl[i], HL_COMMENT, row->size);
            }
        }

        if (in_string) {
            row->hl[i] = HL_STRING;
            if (c == '\\' && i + 1 < row->size) {
                row->hl[i + 1] = HL_STRING;
                i++;
                continue;
            }
            if (c == in_string) in_string = 0;
            prev_sep = 1;
            continue;
        } else if (c == '"' || c == '\'') {
            in_string = c;
            row->hl[i] = HL_STRING;
            continue;
        } else if (c == '{' || c == '}' || c == '(' || c == ')') {
            row->hl[i] = HL_BRACKET;
        } else if (c == '[' || c == ']') {
            row->hl[i] = HL_BRACKET_2;
        }
        if (isdigit(c) || (!prev_sep && row->hl[i - 1] == HL_NUMBER && c == '.')) {
            row->hl[i] = HL_NUMBER;
            prev_sep = c == '.' ? 1 : 0;
            continue;
        } else if (is_separator(c)) {
            prev_sep = 1;
            continue;
        }

        if (prev_sep) {
            int j;
            for (j = 0; keywords1[j]; j++) {
                int klen = strlen(keywords1[j]);

                if (!strncmp(&row->chars[i], keywords1[j], klen) &&
                    is_separator(row->chars[i + klen])) {
                    memset(&row->hl[i], HL_KEYWORD1, klen);
                    i += klen;
                    break;
                }
            }
            if (keywords1[j] != NULL) {
                prev_sep = 0;
                continue;
            }
            for (j = 0; keywords2[j]; j++) {
                int klen = strlen(keywords2[j]);

                if (!strncmp(&row->chars[i], keywords2[j], klen) &&
                    is_separator(row->chars[i + klen])) {
                    memset(&row->hl[i], HL_KEYWORD2, klen);
                    i += klen;
                    break;
                }
            }
            if (keywords2[j] != NULL) {
                prev_sep = 0;
                continue;
            }
        }
        prev_sep = is_separator(c);
    }
}

int syntax_to_color(int hl) {
    switch (hl) {
        case HL_COMMENT: return color_to_ansi(121, 189, 148);
        case HL_STRING: return color_to_ansi(186, 120, 58);
        case HL_BRACKET: return color_to_ansi(255, 255, 0);
        case HL_BRACKET_2: return color_to_ansi(50, 120, 180);
        case HL_KEYWORD1: return color_to_ansi(255, 145, 200);
        case HL_KEYWORD2: return color_to_ansi(50, 120, 180);
        case HL_MATCH: return color_to_ansi(255, 255, 200);
        case HL_NUMBER: return color_to_ansi(140, 200, 100);
        default: return color_to_ansi(255, 255, 255);
    }
}

void select_syntax_hightlight() {
    E.syntax = NULL;
    if (E.filename == NULL) return;

    char *ext = strrchr(E.filename, '.');

    for (size_t j = 0; j < HLDB_ENTRIES; j++) {
        struct Syntax *s = &HLDB[j];
        size_t i = 0;
        while (s->filematch[i]) {
            int is_ext = (s->filematch[i][0] == '.');
            if ((is_ext && ext && !strcmp(ext, s->filematch[i])) ||
                (!is_ext && strstr(E.filename, s->filematch[i]))) {
                E.syntax = s;

                for (int filerow = 0; filerow < E.numrows; filerow++) {
                    update_syntax(&E.rows[filerow]);
                }
                return;
            }
            i++;
        }
    }
}

void append_row(int at, char *s, size_t len) {
    if (at < 0 || at > E.numrows) return;

    E.rows = (Row*) realloc(E.rows, sizeof(Row) * (E.numrows + 1));
    memmove(&E.rows[at + 1], &E.rows[at], sizeof(Row) * (E.numrows - at));

    E.rows[at].size = len;
    E.rows[at].chars = (char*) malloc(len + 1);
    E.rows[at].hl = NULL;
    memcpy(E.rows[at].chars, s, len);
    E.rows[at].chars[len] = '\0';

    update_syntax(&E.rows[at]);

    E.numrows++;
    E.dirty++;
}

void delete_row(int at) {
    if (at < 0 || at >= E.numrows) return;
    free_row(&E.rows[at]);
    memmove(&E.rows[at], &E.rows[at + 1], sizeof(Row) * (E.numrows - at - 1));
    E.numrows--;
    E.dirty++;
}

void insert_char(int c) {
    if (E.cy == E.numrows) {
        append_row(E.numrows, "", 0);
    }
    row_insert_char(&E.rows[E.cy], E.cx, c);
    update_syntax(&E.rows[E.cy]);
    E.cx++;
    E.dirty++;
}

void insert_new_line() {
    if (E.cx == 0) {
        append_row(E.cy, "", 0);
    } else {
        Row *row = &E.rows[E.cy];
        append_row(E.cy + 1, &row->chars[E.cx], row->size - E.cx);
        row = &E.rows[E.cy];
        row->size = E.cx;
        row->chars[row->size] = '\0';
    }
    E.cy++;
    E.cx = 0;
}

void row_delete_char(Row *row, int at) {
    if (at < 0 || at >= row->size) return;
    memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
    row->size--;
    E.dirty++;
}

void row_append_string(Row *row, char *s, size_t len) {
    row->chars = (char*) realloc(row->chars, row->size + len + 1);
    memcpy(&row->chars[row->size], s, len);
    row->size += len;
    row->chars[row->size] = '\0';
    E.dirty++;
}

void delete_char() {
    if (E.cy == E.numrows) return;  
    if (E.cx == 0 && E.cy == 0) return;

    Row *row = &E.rows[E.cy];
    if (E.cx > 0) {
        row_delete_char(row, E.cx - 1);
        update_syntax(row);
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

    char *buf = (char*) malloc(totlen);
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

    select_syntax_hightlight();

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

void esave() {
    if (E.filename == NULL) {
        E.filename = eprompt("Save as: %s (ESC to cancel)", NULL);
        if (E.filename == NULL) {
            set_status_message("Save aborted");
            return;
        }
        select_syntax_hightlight();
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

void scroll() {
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
        E.coloff = E.cx - E.screencols + 1;
    }
}

void draw_rows(Buffer *ab) {
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
                    b_append(ab, "~", 1);
                padding--;
                }
                while (padding--) b_append(ab, " ", 1);
                b_append(ab, welcome, welcomelen);
            } else {
                b_append(ab, "~", 1);
            }
        } else {
            int len = E.rows[filerow].size - E.coloff;
            if (len < 0) len = 0;
            if (len > E.screencols) len = E.screencols;

            char *c = &E.rows[filerow].chars[E.coloff];
            unsigned char *hl = &E.rows[filerow].hl[E.coloff];
            int current_color = -1;

            for (int j = 0; j < len; j++) {
                if (hl[j] == HL_NORMAL) {
                    if (current_color != -1) {
                        b_append(ab, "\x1b[39m", 5);
                        b_append(ab, "\x1b[49m", 5);
                        current_color = -1;
                    }
                } else {
                    int color = syntax_to_color(hl[j]);
                    if (color != current_color) {
                        current_color = color;
                        char buf[16];
                        int clen = snprintf(buf, sizeof(buf), "\x1b[38;5;%dm", color);
                        b_append(ab, buf, clen);
                    }
                }
                b_append(ab, &c[j], 1);
            }
            b_append(ab, "\x1b[39m", 5);
            b_append(ab, "\x1b[49m", 5);
        }
        b_append(ab, "\x1b[K", 3);
        b_append(ab, "\r\n", 2);
    }
}

void draw_status_bar(Buffer *ab) {
    b_append(ab, "\x1b[7m", 4);
    char status[80], rstatus[80];
    int len = snprintf(status, sizeof(status), "%.20s - %d lines %s",
        E.filename ? E.filename : "[No Name]", E.numrows,
        E.dirty ? "(modified)" : "");
    int rlen = snprintf(rstatus, sizeof(rstatus), "%d:%d",
        E.cy + 1, E.numrows);
    if (len > E.screencols) len = E.screencols;
    b_append(ab, status, len);
    while (len < E.screencols) {
        if (E.screencols - len == rlen) {
            b_append(ab, rstatus, rlen);
            break;
        } else {
            b_append(ab, " ", 1);
            len++;
        }
    }
    b_append(ab, "\x1b[m", 3);
    b_append(ab, "\r\n", 2);
}

void draw_message_bar(Buffer *ab) {
    b_append(ab, "\x1b[K", 3);
    int msglen = strlen(E.statusmsg);
    if (msglen > E.screencols) msglen = E.screencols;
    if (msglen && time(NULL) - E.statusmsg_time < 5) {
        b_append(ab, E.statusmsg, msglen);
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

    Buffer ab = ABUF_INIT;
    
    b_append(&ab, "\x1b[?25l", 6);
    b_append(&ab, "\x1b[H", 3);

    draw_rows(&ab);
    draw_status_bar(&ab);
    draw_message_bar(&ab);

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1,
                                              (E.cx - E.coloff) + 1);
    b_append(&ab, buf, strlen(buf));

    b_append(&ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO, ab.buffer, ab.length);
    b_free(&ab);
}

char *eprompt(char *prompt, void (*callback)(char*, int)) {
    size_t bufsize = 128;
    char *buf = (char*) malloc(bufsize);

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
            reset_syntax();
            free(buf);
            return NULL;
        } else if (c == '\r') {
            if (buflen != 0) {
                set_status_message("");
                if (callback) callback(buf, c);
                reset_syntax();
                return buf;
            }
        } else if (!iscntrl(c) && c < 128) {
            if (buflen == bufsize - 1) {
                bufsize += 2;
                buf = (char*) realloc(buf, bufsize);
            }
            buf[buflen++] = c;
            buf[buflen] = '\0';
        }
        if (callback) callback(buf, c);
    }
}

void move_cursor(int key) {
    Row *row = (E.cy >= E.numrows) ? NULL : &E.rows[E.cy];

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

void reset_syntax() {
    for (int i = 0; i < E.numrows; i++) {
        update_syntax(&E.rows[i]);
    }
}

void find_callback(char *c, int i) {
    static size_t index;
    static size_t found;

    if ((i == ARROW_RIGHT || i == ARROW_DOWN) && found != 0) {
        if (index < found - 1) {
            index++;
        } else {
            index = 0;
        }
    } else if ((i == ARROW_LEFT || i == ARROW_UP) && found != 0) {
        if (index > 0) {
            index--;
        } else {
            index = found - 1;
        }
    } else {
        clear_finder(&F);
        for (int i = 0; i < E.numrows; i++) {
            find_in_row(&F, i, E.rows[i].chars, c);
            found = F.found;
            if (found == 0) {
                index = 0;
            }
        }
    }

    reset_syntax();

    Index found_index = F.indexes[index];
    E.cy = found_index.row;
    E.cx = found_index.i;

    for (size_t i = 0; i < F.found; i++) {
        Index index = F.indexes[i];
        Row *row = &E.rows[index.row];
        memset(&row->hl[index.i], HL_MATCH, strlen(c));
    }
}

void efind() {
    char *query = eprompt("Find: %s", find_callback);
    if (query) {
        free(query);
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
            E.cy = E.rowoff;
            break;
        case PAGE_DOWN:
            E.cy = E.rowoff + E.screenrows - 1;
            if (E.cy > E.numrows) E.cy = E.numrows;
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

        case '\t':
            for (int i = 0; i < TAB_SIZE; i++) {
                insert_char(' ');
            }
        break;

        default:
            insert_char(c);
            break;
    }
    quit_times = QUIT_TIMES;
}

void update_window_size(void) {
    if (get_window_size(&E.screenrows, &E.screencols) == -1) die("get_window_size");
    E.screenrows -= 2;
}

void handlesigwinch(int unused __attribute__((unused))) {
    update_window_size();
    if (E.cx > E.screencols) E.cx = E.screencols - 1;
    if (E.cy > E.screenrows) E.cy = E.screenrows - 1;
    refresh_screen();
}

void init_editor() {
    E.cx = 0;
    E.cy = 0;
    E.rowoff = 0;
    E.coloff = 0;
    E.numrows = 0;
    E.rows = NULL;
    E.dirty = 0;
    E.filename = NULL;
    E.statusmsg[0] = '\0';
    E.statusmsg_time = 0;
    E.syntax = NULL;

    update_window_size();    
    signal(SIGWINCH, handlesigwinch);
}

int start_editor(int argc, char *argv[]) {
    enable_raw_mode();
    init_editor();
    init_finder();
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