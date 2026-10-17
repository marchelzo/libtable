#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

#include "table.h"
#include "width.h"


void *TABLE_OPTS = &TABLE_OPTS;

static char const *DELIM = "\x1f";

enum {
        RULE_TOP,
        RULE_HEAD,
        RULE_SEP,
        RULE_BOTTOM,
};

struct style {
        char const *v;
        char const *rule[4][4];
};

static struct style const Ascii = {
        "|",
        {
                { "+", "-", "-", "+" },
                { "*", "-", "-", "*" },
                { "|", "-", "|", "|" },
                { "+", "-", "-", "+" },
        },
};

static struct style const Box = {
        "│",
        {
                { "┌", "─", "┬", "┐" },
                { "╞", "═", "╪", "╡" },
                { "├", "─", "┼", "┤" },
                { "└", "─", "┴", "┘" },
        },
};

static inline size_t max(size_t a, size_t b)
{
        return a > b ? a : b;
}

static size_t
colwidth(char const *s, size_t n, int fl)
{
        return term_width(s, n, fl);
}

static int
termflags(struct table const *t)
{
        return (t->opts & TABLE_OPT_AMBIGUOUS_WIDE) ? TERM_AMBIGUOUS_WIDE : 0;
}

static void
fputns(char const *s, size_t n, FILE *f)
{
        while (n --> 0) {
                fputs(s, f);
        }
}

static void
print_rule(char const *const *r, size_t const *max, size_t cols, size_t unit, FILE *f)
{
        size_t i;

        fputs(r[0], f);

        for (i = 0; i < cols; ++i) {
                fputns(r[1], (max[i] + 2) / unit, f);
                fputs((i + 1 < cols) ? r[2] : r[3], f);
        }

        fputc('\n', f);
}

static size_t
find_break(char const *s, size_t max, int fl, bool *hyphen)
{
        size_t n, len, i, k, brk, soft;
        int w, pw, lb, plb;

        *hyphen = false;

        n = term_fit_cols(s, -1, max, fl);

        if (s[n] == '\0' || isspace((unsigned char) s[n])) {
                return n;
        }

        len  = strlen(s);
        brk  = 0;
        soft = 0;

        for (i = term_step(s, len, 0, &pw, &plb, fl); i <= n; i += k) {
                k = term_step(s, len, i, &w, &lb, fl);
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

        i = term_fit_cols(s, -1, max - 1, fl);
        if (i == 0) {
                i = n;
        }

        return i;
}

static void
print_row(char * const *data, size_t const *max, size_t cols, char const *v, int fl, FILE *f)
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
                        fputs(v, f);
                        fputc(' ', f);

                        n = find_break(cells[i], max[i], fl, &hyphen);
                        w = colwidth(cells[i], n, fl);
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

                        fputns(" ", max[i] > w ? max[i] - w : 0, f);
                }
                fputs(v, f);
                fputc('\n', f);
        }
}

bool table_init(struct table *t, ...)
{
        va_list ap;
        char *header, *fmt, *tmp, **tmph;
        size_t i, fmtlen, totalfmtlen;
        int opt;

        totalfmtlen = 0;
        
        t->rows     = 0;
        t->cols     = 0;
        t->alloc    = 0;
        t->data     = NULL;
        t->fmt      = NULL;
        t->headers  = NULL;
        t->opts     = 0;

        va_start(ap, t);

        for (;; ++t->cols) {
                header = va_arg(ap, char *);
                if (header == NULL) {
                        break;
                }

                if (header == TABLE_OPTS) {
                        while ((opt = va_arg(ap, int)) != 0) {
                                switch (opt) {
                                case TABLE_OPT_BOX:             t->opts |= TABLE_OPT_BOX;             break;
                                case TABLE_OPT_AMBIGUOUS_WIDE:  t->opts |= TABLE_OPT_AMBIGUOUS_WIDE;  break;
                                default:                        goto BadOption;
                                }
                        }
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
                t->max[i] = colwidth(t->headers[i], strlen(t->headers[i]), termflags(t));
        }

        return true;

BadOption:
        va_end(ap);
        free(t->headers);
        free(t->fmt);
        return false;
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

                t->max[i] = max(t->max[i], colwidth(row[i], len, termflags(t)));

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
        size_t i, width, avg, trimthrshld, unit, *max;
        struct style const *st = (t->opts & TABLE_OPT_BOX) ? &Box : &Ascii;
        int fl = termflags(t);

        unit = ((t->opts & TABLE_OPT_BOX) && (fl & TERM_AMBIGUOUS_WIDE)) ? 2 : 1;

        if (n < t->cols * 4 + (t->cols + 1) * unit + 2) {
                return false;
        }

        n -= 2;

        max = malloc(t->cols * sizeof *max);
        if (max == NULL) {
                return false;
        }

        width = (t->cols + 1) * unit;
        for (i = 0; i < t->cols; ++i) {
                max[i] = (t->max[i] + unit - 1) / unit * unit;
                width += max[i] + 2;
        }

        avg = n / t->cols;
        trimthrshld = 0;

        while (width > n) {
                bool none = true;
                for (i = 0; i < t->cols; ++i) {
                        if (max[i] > 2 && max[i] + trimthrshld > avg) {
                                max[i] -= unit;
                                width -= unit;
                                none = false;
                        }
                }

                if (none) {
                        trimthrshld += 1;
                }
        }

        print_rule(st->rule[RULE_TOP], max, t->cols, unit, f);
        print_row(t->headers, max, t->cols, st->v, fl, f);
        print_rule(st->rule[RULE_HEAD], max, t->cols, unit, f);

        for (i = 0; i < t->rows; ++i) {
                print_row(t->data[i], max, t->cols, st->v, fl, f);
                if (i + 1 < t->rows) {
                        print_rule(st->rule[RULE_SEP], max, t->cols, unit, f);
                }
        }

        print_rule(st->rule[RULE_BOTTOM], max, t->cols, unit, f);

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
