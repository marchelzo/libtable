#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

#include "table.h"
#include "width.h"


static char const *DELIM = "\x1f";
static char const CORNER = '+';
static char const INTERSECT = '*';

static inline size_t max(size_t a, size_t b)
{
        return a > b ? a : b;
}

static size_t
colwidth(char const *s, size_t n)
{
        return term_width(s, n);
}

static void fputnc(int c, size_t n, FILE *f)
{
        while (n --> 0) {
                fputc(c, f);
        }
}

static size_t
find_break(char const *s, size_t max, bool *hyphen)
{
        size_t n, len, i, k, brk, soft;
        int w, pw, lb, plb;

        *hyphen = false;

        n = term_fit_cols(s, -1, max);

        if (s[n] == '\0' || isspace((unsigned char) s[n])) {
                return n;
        }

        len  = strlen(s);
        brk  = 0;
        soft = 0;

        for (i = term_step(s, len, 0, &pw, &plb); i <= n; i += k) {
                k = term_step(s, len, i, &w, &lb);
                if (k == 0) {
                        break;
                }

                if (isspace((unsigned char) s[i])) {
                        brk = i;
                } else if (pw == 2 || w == 2) {
                        soft = i;
                        if (lb != TERM_LB_NO_START && plb != TERM_LB_NO_END) {
                                brk = i;
                        }
                }

                pw  = w;
                plb = lb;
        }

        if (brk > 0) {
                return brk;
        }

        if (soft > 0) {
                return soft;
        }

        *hyphen = true;

        i = term_fit_cols(s, -1, max - 1);
        if (i == 0) {
                i = n;
        }

        return i;
}

static void
print_row(char * const *data, size_t const *max, size_t cols, FILE *f)
{
        static size_t alloc = 0;
        static char const **cells = NULL;
        size_t i, n, w;
        bool hyphen, finished;

        if (cols > alloc) {
                char const **tmp;
                tmp = realloc(cells, cols * sizeof *cells);
                if (tmp == NULL) {
                        return;
                }
                cells = tmp;
                alloc = cols;
        }

        for (i = 0; i < cols; ++i) {
                cells[i] = data[i];
        }

        for (finished = false; !finished;) {
                finished = true;
                for (i = 0; i < cols; ++i) {
                        fputs("| ", f);

                        n = find_break(cells[i], max[i], &hyphen);
                        w = colwidth(cells[i], n);
                        fwrite(cells[i], 1, n, f);
                        cells[i] += n;

                        if (hyphen) {
                                fputc('-', f);
                        } else {
                                fputc(' ', f);
                                if (isspace((unsigned char) *cells[i])) {
                                        cells[i] += 1;
                                }
                        }

                        if (*cells[i] != '\0') {
                                finished = false;
                        }

                        fputnc(' ', max[i] > w ? max[i] - w : 0, f);
                }
                fputs("|\n", f);
        }
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
                t->max[i] = colwidth(t->headers[i], strlen(t->headers[i]));
        }

        return true;
}

bool table_add(struct table *t, ...)
{
        va_list ap, aq;
        char *buffer, *field, *end, **row;
        size_t i, len;
        int n;

        if (t->rows == t->alloc) {
                char ***tmp;
                t->alloc = t->alloc * 2 + 4;
                tmp      = realloc(t->data, t->alloc * sizeof (char **));
                if (tmp == NULL) {
                        return false;
                }
                t->data = tmp;
        }

        row = malloc(t->cols * sizeof (char *));
        if (row == NULL) {
                return false;
        }

        va_start(ap, t);
        va_copy(aq, ap);
        n = vsnprintf(NULL, 0, t->fmt, ap);
        va_end(ap);

        buffer = (n < 0) ? NULL : malloc(n + 1);
        if (buffer == NULL) {
                va_end(aq);
                free(row);
                return false;
        }

        vsnprintf(buffer, n + 1, t->fmt, aq);
        va_end(aq);

        field = buffer;

        for (i = 0; i < t->cols; ++i) {
                end = strchr(field, *DELIM);
                len = (end != NULL) ? (size_t) (end - field) : strlen(field);

                row[i] = malloc(len + 1);
                if (row[i] == NULL) {
                        goto err;
                }
                memcpy(row[i], field, len);
                row[i][len] = '\0';

                t->max[i] = max(t->max[i], colwidth(row[i], len));

                field += (end != NULL) ? len + 1 : len;
        }

        free(buffer);

        t->data[t->rows++] = row;

        return true;

err:
        while (i --> 0) {
                free(row[i]);
        }

        free(row);
        free(buffer);

        return false;
}

bool table_print(struct table const *t, size_t n, FILE *f)
{
        size_t i, width, avg, trimthrshld, *max;

        if (n < t->cols * 5 + 3) {
                return false;
        }

        n -= 2;

        max = malloc(t->cols * sizeof *max);
        if (max == NULL) {
                return false;
        }

        width = t->cols * 3 + 1;
        for (i = 0; i < t->cols; ++i) {
                max[i] = t->max[i];
                width += t->max[i];
        }

        avg = n / t->cols;
        trimthrshld = 0;

        while (width > n) {
                bool none = true;
                for (i = 0; i < t->cols; ++i) {
                        if (max[i] > 2 && max[i] + trimthrshld > avg) {
                                max[i] -= 1;
                                width -= 1;
                                none = false;
                        }
                }

                if (none) {
                        trimthrshld += 1;
                }
        }

        fputc(CORNER, f);
        fputnc('-', width - 2, f);
        fputc(CORNER, f);
        fputc('\n', f);

        print_row(t->headers, max, t->cols, f);

        fputc(INTERSECT, f);
        fputnc('-', width - 2, f);
        fputc(INTERSECT, f);
        fputc('\n', f);

        for (i = 0; i < t->rows; ++i) {
                print_row(t->data[i], max, t->cols, f);

                if (i + 1 < t->rows) {
                        size_t j;
                        fputc('|', f);
                        for (j = 0; j < t->cols; ++j) {
                                fputnc('-', max[j] + 2, f);
                                fputc('|', f);
                        }
                        fputc('\n', f);
                }
        }

        fputc(CORNER, f);
        fputnc('-', width - 2, f);
        fputc(CORNER, f);
        fputc('\n', f);

        free(max);
        return true;
}

void table_free(struct table *t)
{
        size_t i, j;

        for (i = 0; i < t->rows; ++i) {
                for (j = 0; j < t->cols; ++j) {
                        free(t->data[i][j]);
                }
        }

        for (i = 0; i < t->rows; ++i) {
                free(t->data[i]);
        }

        free(t->data);
        free(t->headers);
        free(t->max);
        free(t->fmt);
}
