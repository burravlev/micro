#ifndef _EDITOR
#define _EDITOR

#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include "finder.h"
#include "row.h"
#include "terminal.h"
#include "buffer.h"

#define HL_HIGHLIGHT_NUMBERS (1 << 0)
#define HL_HIGHLIGHT_STRINGS (1 << 1)

typedef struct Syntax {
    char *filetype;
    char **filematch;
    char **keywords;
    char *singleline_comment_start;
    int flags;
} Syntax;

enum Highlight {
    HL_NORMAL = 0,
    HL_STRING,
    HL_COMMENT,
    HL_KEYWORD1,
    HL_KEYWORD2,
    HL_NUMBER,
    HL_MATCH,
    HL_BRACKET,
    HL_BRACKET_2,
};

static char *C_HL_extensions[] = {".c", ".h", ".cpp", NULL};
static char *keywords1[] = {
    "switch", "if", "while", "for", "break", "continue", "return", "else",
    "struct", "union", "typedef", "static", "enum", "class", "case", "#include", 
    "#define", "#ifndef", "#endif", NULL
};

static char *keywords2[] = {
    "int", "long", "double", "float", "char", "unsigned", "signed",
    "void", "NULL", NULL
};

static int is_string = 0;

static struct Syntax HLDB[] = {
    {
        "c",
        C_HL_extensions,
        keywords1,
        "//",
        HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS
    },
};

typedef struct Editor {
    int cx, cy;
    int rowoff;
    int coloff;
    int screenrows;
    int screencols;
    int numrows;
    Row *rows;
    int dirty;
    char *filename;
    char statusmsg[80];
    time_t statusmsg_time;
    struct Syntax *syntax;
    struct termios orig_termios;
} Editor;

#define HLDB_ENTRIES (sizeof(HLDB) / sizeof(HLDB[0]))

struct Editor E;

void set_status_message(const char *fmt, ...);
void refresh_screen();
char *eprompt(char *prompt, void (*callback)(char *, int));
void reset_syntax();
int is_separator(int c);
void update_syntax(Row *row);
void select_syntax_hightlight();
void append_row(int at, char *s, size_t len);
void delete_row(int at);
void insert_char(int c);
void insert_new_line();
void delete_char();
void eopen(char *filename);
void esave();
void scroll();
void draw_rows(Buffer *ab);
void draw_status_bar(Buffer *ab);
void draw_message_bar(Buffer *ab);
void set_status_message(const char *fmt, ...);
void refresh_screen();
char *eprompt(char *prompt, void (*callback)(char*, int));
void move_cursor(int key);
void reset_syntax();
void find_callback(char *c, int i);
void efind();
void process_keypress();
void update_window_size(void);
void handlesigwinch(int unused __attribute__((unused)));
void init_editor();
int start_editor(int argc, char *argv[]);

#endif