#ifndef LIBTABLE_H_INCLUDED
#define LIBTABLE_H_INCLUDED

#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>

struct table {
        size_t cols;
        size_t rows;
        size_t alloc;
        size_t *max;
        char ***data;
        char **headers;
        char *fmt;
};

bool table_init(struct table *t, ...);
bool table_add(struct table *t, ...);
bool table_printn(struct table const *t, size_t n, FILE *f);
void table_print(struct table const *t, FILE *f);
void table_free(struct table *t);

#endif
