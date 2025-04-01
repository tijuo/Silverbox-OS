.PHONY: all, clean

CFLAGS  =-O2 -Wall -Wextra -fanalyzer -I../../../../include -I../../../../lib/pdclib/include
all: string_test

clean:
	rm -f string_test

string_test: string_test.c tests.c ../../string.c ../../allocator.c
	$(CC) $(CFLAGS) $+ -o $@
