## libtable

libtable is a tiny C library for generating
pretty-printed ascii tables.

### Example usage

```c
#include "table.h"

int main(void) {
        struct table t;

        table_init(
                &t,
                "Name",  "%-12s",
                "Age",   "%d",
                "Score", "%.2f",
                NULL
        );

        table_add(&t, "Bob", 18, 1.3123);
        table_add(&t, "Alice", 20, 6.43);
        table_add(&t, "Roger", 18, 12.45);
        table_add(&t, "Larry", 59, 12.52);

        table_print(&t, stdout);

        return 0;
}
```

#### Output

```
+----------------------------+
| Name         | Age | Score |
*----------------------------*
| Bob          | 18  | 1.31  |
| Alice        | 20  | 6.43  |
| Roger        | 18  | 12.45 |
| Larry        | 59  | 12.52 |
+----------------------------+
```
