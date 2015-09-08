CFLAGS = -std=c11
CFLAGS += -g3
CFLAGS += -Wall
CFLAGS += -Wextra

all: libtable.a

libtable.a: table.c
	$(CC) $(CFLAGS) -c -o libtable.a $<

install: libtable.a
	cp libtable.a /usr/local/lib/libtable.a
