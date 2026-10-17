#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../width.c"

static isize
encode(u8 *out, i32 cp)
{
        if (cp < 0x80) {
                out[0] = cp;
                return 1;
        }

        if (cp < 0x800) {
                out[0] = 0xC0 | (cp >> 6);
                out[1] = 0x80 | (cp & 0x3F);
                return 2;
        }

        if (cp < 0x10000) {
                out[0] = 0xE0 | (cp >> 12);
                out[1] = 0x80 | ((cp >> 6) & 0x3F);
                out[2] = 0x80 | (cp & 0x3F);
                return 3;
        }

        out[0] = 0xF0 | (cp >> 18);
        out[1] = 0x80 | ((cp >> 12) & 0x3F);
        out[2] = 0x80 | ((cp >> 6) & 0x3F);
        out[3] = 0x80 | (cp & 0x3F);
        return 4;
}

static bool
check(u8 const *s, isize n, bool const *brk, bool fast)
{
        isize i, m;
        int w;

        for (i = 0; i < n; i += m) {
                m = fast ? term_cluster(s, n, i, &w) : Cluster(s, n, i, &w);
                if (!brk[i]) {
                        return false;
                }
                for (isize j = i + 1; j < i + m; ++j) {
                        if (brk[j]) {
                                return false;
                        }
                }
        }

        return true;
}

int
main(int argc, char **argv)
{
        FILE *f;
        char line[4096], *p, *end;
        u8 s[1024];
        bool brk[1025];
        isize n;
        int total = 0, bad = 0;
        bool next;

        if (argc != 2 || (f = fopen(argv[1], "r")) == NULL) {
                fprintf(stderr, "usage: %s GraphemeBreakTest.txt\n", argv[0]);
                return 2;
        }

        while (fgets(line, sizeof line, f) != NULL) {
                if (line[0] == '#' || line[0] == '\n') {
                        continue;
                }

                n    = 0;
                next = false;

                for (p = line; *p != '\0' && *p != '#';) {
                        if (strncmp(p, "÷", strlen("÷")) == 0) {
                                next = true;
                                p += strlen("÷");
                        } else if (strncmp(p, "×", strlen("×")) == 0) {
                                next = false;
                                p += strlen("×");
                        } else if (isxdigit((unsigned char) *p)) {
                                i32 cp = strtol(p, &end, 16);
                                isize k = encode(s + n, cp);
                                brk[n] = next;
                                for (isize j = 1; j < k; ++j) {
                                        brk[n + j] = false;
                                }
                                n += k;
                                p = end;
                        } else {
                                p += 1;
                        }
                }

                total += 1;

                if (!check(s, n, brk, false) || !check(s, n, brk, true)) {
                        bad += 1;
                        fputs(line, stdout);
                }
        }

        printf("%d/%d passed\n", total - bad, total);

        return bad != 0;
}
