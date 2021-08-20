ARFLAGS =rs

DIRS=libos libc

.PHONY:	$DIRS all dep depend clean

all: libc/libc.a libos/libos.a

install:

libc/libc.a:
	make -C libc all

libos/libos.a:
	make -C libos all

dep:
	for i in $(DIRS); do make -C $$i depend; done

depend:
	for i in $(DIRS); do make -C $$i depend; done

clean:
	rm -f libc/libc.a libos/libos.a
	rm -f *.a
	for i in $(DIRS); do make -C $$i clean; done
	rm -rf *.o
