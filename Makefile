CFLAGS = -std=c11
CFLAGS += -O2
CFLAGS += -funroll-loops
CFLAGS += -flto
CFLAGS += -Wall
CFLAGS += -Wextra

OBJS = table.o width.o

all: libtable.a

libtable.a: $(OBJS)
	$(AR) rcs $@ $(OBJS)

table.o: table.c table.h width.h
width.o: width.c width.h width_data.inc

example: example.c libtable.a
	$(CC) $(CFLAGS) -o $@ example.c libtable.a

tools/unicode/gbtest: tools/unicode/gbtest.c width.c width.h width_data.inc
	$(CC) -std=c11 -Wall -Wextra -o $@ tools/unicode/gbtest.c

check: tools/unicode/gbtest
	tools/unicode/gbtest $(UCD)/auxiliary/GraphemeBreakTest.txt

clean:
	rm -f $(OBJS) libtable.a example tools/unicode/gbtest

install:
	cp libtable.a /usr/local/lib/libtable.a
	cp table.h /usr/local/include/table.h
