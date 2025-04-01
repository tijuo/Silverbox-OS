include ../Makefile.inc

.PHONY: all tests clean libos libc-tests libos-tests \
libc-clean libos-clean librust-clean librust-tests librust \
pdclib pdclib-clean

all: libos pdclib # librust

libc:
	make -C libc all

libos:
	make -C libos all
	cp libos/libos.a libos.a
	cp libos/libos_init.a libos_init.a

#librust:
#	make -C librust all

#librust-tests:
#	make -C librust tests

#libc-tests:
#	make -C libc tests

libos-tests:
	make -C libos tests

pdclib: libc.a

libc.a: pdclib/build/libpdclib.a
	cp pdclib/build/libpdclib.a libc.a

pdclib/build/libpdclib.a:
	CFLAGS="-std=gnu17 -fno-leading-underscore -ffreestanding -nostartfiles -fno-stack-protector -fno-stack-check -fno-asynchronous-unwind-tables -I$(ROOT_DIR)/lib/pdclib/include -I$(ROOT_DIR)/include -I$(COMPILER_INCL)/include" cmake -B pdclib/build pdclib -DUSE_THREADS=off && make -j 12 -C pdclib/build pdclibs

pdclib-clean:
	rm -rf pdclib/build

crt0.o: crt0.c
	$(CC) -c crt0.c -o crt0.o

tests: libos-tests#libc-tests libos-tests librust-tests

clean: pdclib-clean libos-clean #libc-clean librust-clean
	rm -f crt0.o
	rm -f libc.a
	rm -f libos.a
	rm -f libos_init.a

#libc-clean:
#	make -C libc clean

libos-clean:
	make -C libos clean

#librust-clean:
#	make -C librust clean
