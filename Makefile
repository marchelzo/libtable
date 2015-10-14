CFLAGS = -std=c11
CFLAGS += -O2
CFLAGS += -funroll-loops
CFLAGS += -flto
CFLAGS += -Wall
CFLAGS += -Wextra

all: libtable.a

libtable.a: table.c
	$(CC) $(CFLAGS) -c -o $@ $<

install:
	cp libtable.a /usr/local/lib/libtable.a
	cp table.h /usr/local/include/table.h
