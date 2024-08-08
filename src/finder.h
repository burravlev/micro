#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct position {
    int x, y;
} position;

typedef struct finder {
    position *idx;
    // current position
    int curr;
    int size;
} finder;

void fclean(finder *f) {
    free(f->idx); 
    f->idx = malloc(sizeof(position));
    f->curr = 0;
    f->size = 0;
}

void finit(finder *f) {
    f->idx = malloc(sizeof(position));
    f->curr = 0;
    f->size = 0;
}

int ffind(finder *f, char *s, char *query, int row_num) {
    if (strlen(query) == 0) {
        return 0;
    }
    int *x = (int *) malloc(sizeof(int));
    int count = 0;
    char *ptr = s;
    int l = strlen(query);

    while ((ptr = strstr(ptr, query)) != NULL) {
        x = (int *) realloc(x, sizeof(int) * (count + 1));
        x[count] = ptr - s;
        ptr += l;
        count++;
    }

    position *p = (position *) malloc(sizeof(position) * count);
    f->idx = (position *) realloc(f->idx, f->size + count);

    for (int i = 0; i < count; i++) {
        p[i].x = x[i];
        p[i].y = row_num;
        f->idx[f->size++] = p[i];
    }    

    return count;
}

void fnext(finder *f, int *x, int *y) {
    if (f->size == 0) {
        return;
    }
    if (f->curr == f->size - 1) {
        f->curr = 0;
    } else {
        f->curr++;
    }
    *x = f->idx[f->curr].x;
    *y = f->idx[f->curr].y;
}

void fprev(finder *f, int *x, int *y) {
    if (f->size == 0) {
        return;
    }
    if (f->curr == 0) {
        f->curr = f->size - 1;
    } else {
        f->curr--;
    }
    *x = f->idx[f->curr].x;
    *y = f->idx[f->curr].y;
}