## libtable

libtable is a tiny C library for generating
pretty-printed ascii tables.

### Installation

```sh
git clone https://github.com/marchelzo/libtable.git && \
cd libtable && \
make && sudo make install
```

### Example usage

```c
#include <table.h>

int main(void) {
        struct table t;

        table_init(
                &t,
                "Name",  "%s",
                "Age",   "%d",
                "Score", "%.2f",
                NULL
        );

        table_add(&t, "Amet fugiat commodi eligendi possimus harum earum. "
                      "Sequi quidem ab commodi tempore mollitia provident. "
                      "Iusto incidunt consequuntur rem eligendi illum. "
                      "Nisi odit soluta dolorum vero enim neque id. Hic magni? "
                      "foo bar baz", 36, 3.1);
        table_add(&t, "Ametfugiatcommodieligendipossimusharumearum."
                      "Sequiquidemabcommoditemporemollitiaprovident."
                      "Iustoinciduntconsequunturremeligendiillum."
                      "Nisioditsolutadolorumveroenimnequeid.Hicmagni?"
                      "foobarbaz",36,3.1);
        table_add(&t, "Ξεσκεπάζωτὴνψυχοφθόραβδελυγμία", 36, 3.1);
        table_add(&t, "Ξεσκεπάζω τὴν ψυχοφθόρα βδελυγμία", 36, 3.1);
        table_add(&t, "Bob", 18, 1.3123);
        table_add(&t, "Alice", 20, 6.43);
        table_add(&t, "Roger", 18, 12.45);
        table_add(&t, "Larry", 59, 12.52);
        table_add(&t, "Ё Ђ Ѓ Є Ѕ І Ї Ј Љ", 21, 14.12312312);

        table_print(&t, 60, stdout);

        table_free(&t);

        return 0;
}
```

#### Output

```
+--------------------------------------------------------+
| Name                                     | Age | Score |
*--------------------------------------------------------*
| Amet fugiat commodi eligendi possimus    | 36  | 3.10  |
| harum earum. Sequi quidem ab commodi     |     |       |
| tempore mollitia provident. Iusto        |     |       |
| incidunt consequuntur rem eligendi       |     |       |
| illum. Nisi odit soluta dolorum vero     |     |       |
| enim neque id. Hic magni? foo bar baz    |     |       |
|------------------------------------------|-----|-------|
| Ametfugiatcommodieligendipossimusharume- | 36  | 3.10  |
| arum.Sequiquidemabcommoditemporemolliti- |     |       |
| aprovident.Iustoinciduntconsequunturrem- |     |       |
| eligendiillum.Nisioditsolutadolorumvero- |     |       |
| enimnequeid.Hicmagni?foobarbaz           |     |       |
|------------------------------------------|-----|-------|
| Ξεσκεπάζωτὴνψυχοφθόραβδελυγμία           | 36  | 3.10  |
|------------------------------------------|-----|-------|
| Ξεσκεπάζω τὴν ψυχοφθόρα βδελυγμία        | 36  | 3.10  |
|------------------------------------------|-----|-------|
| Bob                                      | 18  | 1.31  |
|------------------------------------------|-----|-------|
| Alice                                    | 20  | 6.43  |
|------------------------------------------|-----|-------|
| Roger                                    | 18  | 12.45 |
|------------------------------------------|-----|-------|
| Larry                                    | 59  | 12.52 |
|------------------------------------------|-----|-------|
| Ё Ђ Ѓ Є Ѕ І Ї Ј Љ                        | 21  | 14.12 |
+--------------------------------------------------------+
```
