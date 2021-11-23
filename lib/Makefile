include ../Makefile.inc

.PHONY: all tests clean libc libos libc-tests libos-tests \
libc-clean libos-clean librust-clean librust-tests librust

all: libc libos librust

libc:
	make -C libc all

libos:
	make -C libos all

librust:
	make -C librust all

librust-tests:
	make -C librust tests

libc-tests:
	make -C libc tests

libos-tests:
	make -C libos tests

tests: libc-tests libos-tests librust-tests

clean: libc-clean libos-clean librust-clean

libc-clean:
	make -C libc clean

libos-clean:
	make -C libos clean

librust-clean:
	make -C librust clean
