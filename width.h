#ifndef LIBTABLE_WIDTH_H_INCLUDED
#define LIBTABLE_WIDTH_H_INCLUDED

#include <stdint.h>
#include <stdbool.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t  i32;
typedef int64_t  isize;

enum {
        TERM_AMBIGUOUS_WIDE = 1,
};

isize
term_width(void const *s, isize n, int fl);

isize
term_fit_cols(void const *s, isize n, int cols, int fl);

enum {
        TERM_LB_ANY,
        TERM_LB_NO_START,
        TERM_LB_NO_END,
};

isize
term_step(void const *s, isize n, isize i, int *w, int *lb, int fl);

#endif
