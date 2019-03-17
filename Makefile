.POSIX:
CC      = cc
CFLAGS  = -ansi -pedantic -Wall -Wextra -Wno-missing-field-initializers \
    -O3 -ggdb3 -fsanitize=address -fsanitize=undefined
LDFLAGS =
LDLIBS  =

tests/tests: tests/tests.c bencode.c bencode.h
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ tests/tests.c bencode.c $(LDLIBS)

check: tests/tests
	tests/tests

clean:
	rm -f tests/tests
