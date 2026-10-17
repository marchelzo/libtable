#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "table.h"

int
main(int argc, char **argv)
{
        struct table t;
        size_t width = 72;
        int opts[3] = { 0 };
        int nopts = 0;
        bool box = false;
        bool amb = false;
        int i;

        for (i = 1; i < argc; ++i) {
                if (strcmp(argv[i], "-b") == 0) {
                        box = true;
                } else if (strcmp(argv[i], "-a") == 0) {
                        amb = true;
                } else {
                        width = strtoul(argv[i], NULL, 10);
                }
        }

        if (box) {
                opts[nopts++] = TABLE_OPT_BOX;
        }

        if (amb) {
                opts[nopts++] = TABLE_OPT_AMBIGUOUS_WIDE;
        }

        if (
                !table_init(
                        &t,
                        "Name",     "%s",
                        "Lang",     "%s",
                        "Greeting", "%s",
                        "Score",    "%.1f",
                        TABLE_OPTS, opts[0], opts[1], 0
                )
        ) {
                return 1;
        }

        table_add(&t, "Alice",   "English",  "Hello, world! Nice to meet you.",       9.5);
        table_add(&t, "张伟",    "中文",     "你好，世界！很高兴认识你。",            8.7);
        table_add(&t, "さくら",  "日本語",   "こんにちは、世界！🌸 はじめまして。",   7.0);
        table_add(&t, "민준",    "한국어",   "안녕하세요, 세계! 만나서 반갑습니다.",  9.3);
        table_add(&t, "Zoë",     "Français", "Bonjour le monde ! Enchanté. 🇫🇷",       6.5);
        table_add(&t, "Family",  "Emoji",    "👨‍👩‍👧‍👦 🎉 👍🏽 ❤️ 1️⃣ — all one cell each",    10.0);

        if (!table_print(&t, width, stdout)) {
                fprintf(stderr, "%s: width %zu is too narrow\n", argv[0], width);
                table_free(&t);
                return 1;
        }

        table_free(&t);

        return 0;
}
