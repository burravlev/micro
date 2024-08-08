#include <stdio.h>

enum Level {
    INFO,
    DEBUG,
    ERROR,
};

FILE *f;

void linit() {
    f = fopen("logs.log", "ab+");
    printf("%d", f);
}

void lexit() {
    fclose(f);
}