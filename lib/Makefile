include ../Makefile.inc

.PHONY: all tests clean libc libos libc-tests libos-tests \
libc-clean libos-clean

all: libc libos

libc:
	make -C libc all

libos:
	make -C libos all

libc-tests:
	make -C libc tests

libos-tests:
	make -C libos tests

tests: libc-tests libos-tests

clean: libc-clean libos-clean

libc-clean:
	make -C libc clean

libos-clean:
	make -C libos clean
