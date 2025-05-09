#ifndef TERMINAL_H
#define TERMINAL_H

#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <stddef.h>

#define ESC_CHAR '\x1b'
#define STR_END '\0'
#define CLEAR_SCREEN "\x1b[2J"
#define HIDE_CURSOR "\x1b[?25l"
#define SHOW_CURSOR "\x1b[?25h"

#define CTRL_KEY(k) ((k) & 0x1f)

struct termios instance;

void die(const char* s);
void disable_raw_mode();
void enable_raw_mode();
void term_write(const char *s, size_t size);
int read_key();
int get_window_size(int *rows, int *cols);

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

#endif