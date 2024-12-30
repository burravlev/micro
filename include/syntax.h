#ifndef SYNTAX_H
#define SYNTAX_H

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <color.h>

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

char *C_HL_extensions[] = {".c", ".h", ".cpp", NULL};
char *keywords1[] = {
    "switch", "if", "while", "for", "break", "continue", "return", "else",
    "struct", "union", "typedef", "static", "enum", "class", "case", "#include", 
    "#define", "#ifndef", "#endif", NULL
};

char *keywords2[] = {
    "int", "long", "double", "float", "char", "unsigned", "signed",
    "void", "NULL", NULL
};

int is_string = 0;

#endif