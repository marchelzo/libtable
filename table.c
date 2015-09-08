#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

#include "table.h"


static char const *DELIM = "\x1f";
static char const CORNER = '+';
static char const INTERSECT = '*';

static inline size_t max(size_t a, size_t b) {
        return a > b ? a : b;
}

bool table_init(struct table *t, ...)
{
        va_list ap;
        char *header, *fmt, *tmp, **tmph;
        size_t i, fmtlen, totalfmtlen;

        totalfmtlen = 0;
        
        t->rows     = 0;
        t->cols     = 0;
        t->alloc    = 0;
        t->data     = NULL;
        t->fmt      = NULL;
        t->headers  = NULL;

        va_start(ap, t);

        for (;; ++t->cols) {
                header = va_arg(ap, char *);
                if (header == NULL) {
                        break;
                }
                fmt    = va_arg(ap, char *);

                tmph = realloc(t->headers, (t->cols + 1) * sizeof (char *));
                if (tmph == NULL) {
                        free(t->headers);
                        return false;
                }
                t->headers = tmph;

                t->headers[t->cols] = header;

                fmtlen = strlen(fmt);
                tmp = realloc(t->fmt, totalfmtlen + fmtlen + 2);
                if (tmp == NULL) {
                        free(t->fmt);
                        free(t->headers);
                        return false;
                }
                t->fmt = tmp;
                strcpy(t->fmt + totalfmtlen, fmt);
                totalfmtlen += fmtlen;
                strcpy(t->fmt + totalfmtlen, DELIM);
                totalfmtlen += 1;
        }

        va_end(ap);

        t->max = malloc(t->cols * sizeof *t->max);
        if (t->max == NULL) {
                free(t->headers);
                free(t->fmt);
                return false;
        }

        for (i = 0; i < t->cols; ++i) {
                t->max[i] = strlen(t->headers[i]);
        }

        return true;
}

bool table_add(struct table *t, ...)
{
        va_list ap;
        char *field, **row, buffer[512];
        size_t i, len;

        if (t->rows == t->alloc) {
                char ***tmp;
                t->alloc = t->alloc * 2 + 4;
                tmp      = realloc(t->data, t->alloc * sizeof (char **));
                if (tmp == NULL) {
                        return false;
                }
                t->data = tmp;
        }

        row = (t->data[t->rows++] = malloc(t->cols * sizeof (char *)));

        if (row == NULL) {
                t->rows -= 1;
                return false;
        }



        va_start(ap, t);
        vsnprintf(buffer, sizeof buffer, t->fmt, ap);
        va_end(ap);

        field = strtok(buffer, DELIM);

        for (i = 0; field != NULL; ++i, field = strtok(NULL, DELIM)) {
                len = strlen(field);
                row[i] = malloc(len + 1);
                if (row[i] == NULL) {
                        goto err;
                }
                strcpy(row[i], field);

                t->max[i] = max(t->max[i], len);
        }

        return true;

err:
        while (i --> 0) {
                free(row[i]);
        }
        
        return false;
}

void table_print(struct table const *t, FILE *f)
{
        size_t i, j, width;

        width = t->cols * 2 + 2;
        for (i = 0; i < t->cols; ++i) {
                width += t->max[i];
        }

        fputc(CORNER, f);
        for (i = 0; i < width; ++i) {
                fputc('-', f);
        }
        fputc(CORNER, f);
        fputc('\n', f);

        for (i = 0; i < t->cols; ++i) {
                fprintf(
                        f,
                        "| %-*.*s%s",
                        (int) t->max[i],
                        (int) t->max[i],
                        t->headers[i],
                        i + 1 == t->cols ? " |\n" : " "
                );
        }

        fputc(INTERSECT, f);
        for (i = 0; i < width; ++i) {
                fputc('-', f);
        }
        fputc(INTERSECT, f);
        fputc('\n', f);

        for (i = 0; i < t->rows; ++i) {
                for (j = 0; j < t->cols; ++j) {
                        fprintf(
                                f,
                                "| %-*.*s%s",
                                (int) t->max[j],
                                (int) t->max[j],
                                t->data[i][j],
                                j + 1 == t->cols ? " |\n" : " "
                        );
                }
        }

        fputc(CORNER, f);
        for (i = 0; i < width; ++i) {
                fputc('-', f);
        }
        fputc(CORNER, f);
        fputc('\n', f);
}

void table_printn(struct table const *t, size_t n, FILE *f)
{
        size_t i, j;

        fputc(CORNER, f);
        fputc(' ', f);
        for (i = 0; i < t->cols; ++i) {
                fprintf(f, "%s%c", t->headers[i], i + 1 == t->cols ? '\n' : ' ');
        }

        for (i = 0; i < t->rows; ++i) {
                for (j = 0; j < t->cols; ++j) {
                        fprintf(f, "%s%c", t->data[i][j], j + 1 == t->cols ? '\n' : ' ');
                }
        }
}

void table_free(struct table *t)
{
        size_t i;

        for (i = 0; i < t->cols; ++i) {
                free(t->data[i]);
        }

        free(t->data);
        free(t->headers);
        free(t->max);
        free(t->fmt);
}
