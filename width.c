#include <stdint.h>
#include <string.h>

#include "width.h"

#include "width_data.inc"

enum {
        GCB_OTHER,
        GCB_CR,
        GCB_LF,
        GCB_CONTROL,
        GCB_EXTEND,
        GCB_ZWJ,
        GCB_RI,
        GCB_PREPEND,
        GCB_SPACING_MARK,
        GCB_L,
        GCB_V,
        GCB_T,
        GCB_LV,
        GCB_LVT,
};

enum {
        INCB_NONE,
        INCB_LINKER,
        INCB_CONSONANT,
        INCB_EXTEND,
};

#define PWIDTH(p) ((p) & 3)
#define PGCB(p)   (((p) >> 2) & 15)
#define PPICT(p)  (((p) >> 6) & 1)
#define PEMOJI(p) (((p) >> 7) & 1)
#define PINCB(p)  (((p) >> 8) & 3)
#define PLB(p)    (((p) >> 10) & 3)
#define PAMB(p)   (((p) >> 12) & 1)

typedef struct {
        u8 ri;
        u8 pict;
        u8 incb;
} GState;

inline static u16
Props(i32 cp)
{
        isize blk = PropStage1[cp >> PROP_SHIFT];
        isize off = cp & ((1 << PROP_SHIFT) - 1);
        return PropValues[PropStage2[(blk << PROP_SHIFT) | off]];
}

static bool
IsEmoji(i32 cp)
{
        return PEMOJI(Props(cp));
}

inline static bool
IsModifier(i32 cp)
{
        return (cp >= 0x1F3FB) && (cp <= 0x1F3FF);
}

static int
Presentation(i32 base, i32 next, int width)
{
        bool vs = (next == 0xFE0E) || (next == 0xFE0F);

        if (!(vs || IsModifier(next)) || !IsEmoji(base)) {
                return width;
        }

        return (next == 0xFE0E) ? 1 : 2;
}

static isize
term_esc_len(u8 const *s, isize n, isize i)
{
        isize j;

        if (s[i] != 0x1B) {
                return 0;
        }

        if (i + 1 >= n) {
                return 1;
        }

        switch (s[i + 1]) {
        case '[':
                for (j = i + 2; j < n; ++j) {
                        if (s[j] >= 0x40 && s[j] <= 0x7E) {
                                return j + 1 - i;
                        }
                        if (s[j] < 0x20 || s[j] > 0x3F) {
                                return j - i;
                        }
                }
                return n - i;

        case ']':
        case 'P':
        case '_':
        case '^':
        case 'X':
                for (j = i + 2; j < n; ++j) {
                        switch (s[j]) {
                        case 0x07:
                        case 0x18:
                        case 0x1A:
                                return j + 1 - i;

                        case 0x1B:
                                if (j + 1 < n && s[j + 1] == '\\') {
                                        return j + 2 - i;
                                }
                                return j - i;
                        }
                }
                return n - i;
        }

        for (j = i + 1; j < n && s[j] >= 0x20 && s[j] <= 0x2F; ++j) {
                ;
        }

        if (j < n && s[j] >= 0x30 && s[j] <= 0x7E) {
                return j + 1 - i;
        }

        return j - i;
}

inline static i32
Decode(u8 const *s, isize n, isize *len)
{
        i32 cp;

        if (s[0] < 0x80) {
                *len = 1;
                return s[0];
        }

        if (s[0] >= 0xC2 && s[0] <= 0xDF && n >= 2 && (s[1] & 0xC0) == 0x80) {
                *len = 2;
                return ((s[0] & 0x1F) << 6) | (s[1] & 0x3F);
        }

        if (
                (s[0] & 0xF0) == 0xE0
             && (n >= 3)
             && ((s[1] & 0xC0) == 0x80)
             && ((s[2] & 0xC0) == 0x80)
        ) {
                cp = ((s[0] & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
                if (cp >= 0x800 && (cp < 0xD800 || cp > 0xDFFF)) {
                        *len = 3;
                        return cp;
                }
        }

        if (
                (s[0] >= 0xF0 && s[0] <= 0xF4)
             && (n >= 4)
             && ((s[1] & 0xC0) == 0x80)
             && ((s[2] & 0xC0) == 0x80)
             && ((s[3] & 0xC0) == 0x80)
        ) {
                cp = ((s[0] & 0x07) << 18)
                   | ((s[1] & 0x3F) << 12)
                   | ((s[2] & 0x3F) << 6)
                   | (s[3] & 0x3F);
                if (cp >= 0x10000 && cp <= 0x10FFFF) {
                        *len = 4;
                        return cp;
                }
        }

        return -1;
}

static void
Advance(GState *st, u16 p)
{
        int incb = PINCB(p);

        st->ri = (PGCB(p) == GCB_RI) ? st->ri + 1 : 0;

        if (PPICT(p)) {
                st->pict = 1;
        } else if (st->pict == 1 && PGCB(p) == GCB_EXTEND) {
                st->pict = 1;
        } else if (st->pict == 1 && PGCB(p) == GCB_ZWJ) {
                st->pict = 2;
        } else {
                st->pict = 0;
        }

        if (incb == INCB_CONSONANT) {
                st->incb = 1;
        } else if (st->incb != 0 && incb == INCB_LINKER) {
                st->incb = 2;
        } else if (incb != INCB_EXTEND) {
                st->incb = 0;
        }
}

static bool
Break(GState const *st, u16 prev, u16 next)
{
        int a = PGCB(prev);
        int b = PGCB(next);

        if (a == GCB_CR && b == GCB_LF) {
                return false;
        }

        if (
                (a == GCB_CONTROL || a == GCB_CR || a == GCB_LF)
             || (b == GCB_CONTROL || b == GCB_CR || b == GCB_LF)
        ) {
                return true;
        }

        switch (a) {
        case GCB_L:
                if (b == GCB_L || b == GCB_V || b == GCB_LV || b == GCB_LVT) {
                        return false;
                }
                break;

        case GCB_LV:
        case GCB_V:
                if (b == GCB_V || b == GCB_T) {
                        return false;
                }
                break;

        case GCB_LVT:
        case GCB_T:
                if (b == GCB_T) {
                        return false;
                }
                break;
        }

        if (
                (b == GCB_EXTEND)
             || (b == GCB_ZWJ)
             || (b == GCB_SPACING_MARK)
             || (a == GCB_PREPEND)
        ) {
                return false;
        }

        if (st->incb == 2 && PINCB(next) == INCB_CONSONANT) {
                return false;
        }

        if (st->pict == 2 && PPICT(next)) {
                return false;
        }

        if (a == GCB_RI && b == GCB_RI && (st->ri & 1)) {
                return false;
        }

        return true;
}

inline static int
SimpleWidth(i32 cp)
{
        u32 c = (u32)cp;

        if (c < 0x300) {
                return (c >= 0x20 && c != 0x7F && (c < 0x80 || c >= 0xA0)) ? 1 : -1;
        }

        if (c < 0x3000) {
                if (c >= 0x2500) {
                        return (c <= 0x25FC || (c >= 0x2800 && c <= 0x28FF)) ? 1 : -1;
                }
                if (c >= 0x2010) {
                        return (c <= 0x2027) ? 1 : -1;
                }
                return ((c >= 0x370 && c <= 0x482) || (c >= 0x48A && c <= 0x52F)) ? 1 : -1;
        }

        if (c >= 0x4E00) {
                if (
                        (c <= 0x9FFF)
                     || (c >= 0xAC00 && c <= 0xD7A3)
                     || (c >= 0xFF01 && c <= 0xFF60)
                ) {
                        return 2;
                }
                return -1;
        }

        if (
                (c <= 0x3029)
             || (c >= 0x3041 && c <= 0x3096)
             || (c >= 0x30A1 && c <= 0x30FA)
             || (c >= 0x3400 && c <= 0x4DBF)
        ) {
                return 2;
        }

        return -1;
}

static isize
Cluster(u8 const *s, isize n, isize i, int *w, int fl)
{
        GState st = { 0 };
        isize len;
        isize m;
        isize end;
        i32 cp = Decode(s + i, n - i, &len);
        i32 next;
        u16 p;
        u16 q;
        int width;

        if (cp < 0) {
                *w = 1;
                return 1;
        }

        p     = Props(cp);
        width = PWIDTH(p);
        end   = i + len;

        if ((fl & TERM_AMBIGUOUS_WIDE) && PAMB(p)) {
                width = 2;
        }

        Advance(&st, p);

        while (end < n) {
                next = Decode(s + end, n - end, &m);
                if (next < 0) {
                        break;
                }

                q = Props(next);
                if (Break(&st, p, q)) {
                        break;
                }

                if (end == i + len) {
                        width = Presentation(cp, next, width);
                }

                Advance(&st, q);
                p = q;
                end += m;
        }

        *w = width;

        return end - i;
}

static isize
term_cluster(u8 const *s, isize n, isize i, int *w, int fl)
{
        isize m;
        isize m2;
        i32 cp = Decode(s + i, n - i, &m);

        if (cp == '\r' && i + 1 < n && s[i + 1] == '\n') {
                *w = 0;
                return 2;
        }

        if (cp >= 0 && (cp < 0x20 || cp == 0x7F)) {
                *w = 0;
                return 1;
        }

        int sw = (cp >= 0 && !(fl & TERM_AMBIGUOUS_WIDE)) ? SimpleWidth(cp) : -1;

        if (sw > 0) {
                if (i + m >= n) {
                        *w = sw;
                        return m;
                }

                i32 nx = Decode(s + i + m, n - i - m, &m2);

                if (nx >= 0 && (nx < 0x20 || nx == 0x7F || SimpleWidth(nx) > 0)) {
                        *w = sw;
                        return m;
                }
        }

        return Cluster(s, n, i, w, fl);
}

static isize
Run(u8 const *s, isize n, isize i, isize cols, isize *width)
{
        isize acc = *width;
        isize m;
        i32 cp = Decode(s + i, n - i, &m);
        int sw = (cp >= 0) ? SimpleWidth(cp) : -1;

        while (sw > 0 && acc + sw <= cols) {
                isize j  = i + m;
                isize m2 = 0;
                int nsw  = 0;

                if (j < n && s[j] >= 0x80) {
                        i32 nx = Decode(s + j, n - j, &m2);
                        nsw = (nx >= 0) ? SimpleWidth(nx) : -1;
                        if (nsw < 0) {
                                break;
                        }
                }

                acc += sw;
                i  = j;
                m  = m2;
                sw = nsw;
        }

        *width = acc;

        return i;
}

static isize
Fit(u8 const *s, isize n, isize cols, isize *out, int fl)
{
        isize width = 0;
        isize i = 0;

        while (i < n) {
                u8 c = s[i];
                isize m;
                int w;

                if (c == 0x1B) {
                        i += term_esc_len(s, n, i);
                        continue;
                }

                if (c >= 0x20 && c < 0x7F && (i + 1 >= n || s[i + 1] < 0x80)) {
                        m = 1;
                        w = 1;
                } else if (c >= 0x80 && (fl & TERM_AMBIGUOUS_WIDE)) {
                        m = Cluster(s, n, i, &w, fl);
                } else if (c >= 0x80) {
                        isize j = Run(s, n, i, cols, &width);
                        if (j > i) {
                                i = j;
                                continue;
                        }
                        m = Cluster(s, n, i, &w, fl);
                } else {
                        m = term_cluster(s, n, i, &w, fl);
                }

                if (width + w > cols) {
                        break;
                }

                width += w;
                i += m;
        }

        *out = width;

        return i;
}

isize
term_width(void const *s, isize n, int fl)
{
        isize width;

        if (n == -1) {
                n = strlen(s);
        }

        Fit(s, n, INT64_MAX, &width, fl);

        return width;
}

isize
term_fit_cols(void const *s, isize n, int cols, int fl)
{
        isize width;

        if (n == -1) {
                n = strlen(s);
        }

        return Fit(s, n, cols, &width, fl);
}

isize
term_step(void const *s, isize n, isize i, int *w, int *lb, int fl)
{
        u8 const *p = s;
        isize j = i;
        isize len;
        i32 cp;

        while (j < n && p[j] == 0x1B) {
                j += term_esc_len(p, n, j);
        }

        if (j >= n) {
                *w  = 0;
                *lb = TERM_LB_ANY;
                return j - i;
        }

        cp  = Decode(p + j, n - j, &len);
        *lb = (cp >= 0) ? PLB(Props(cp)) : TERM_LB_ANY;

        return j + term_cluster(p, n, j, w, fl) - i;
}
