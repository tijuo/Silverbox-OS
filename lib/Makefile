ARFLAGS =rs

DIRS=libos libc

.PHONY:	$DIRS all dep depend clean

all:
	make libc.a
	make libos.a
	cp libos/libos.a .
	cp libc/libc.a .

install:

libc.a:
	make -C libc all

libos.a:
	make -C libos all

dep:
	for i in $(DIRS); do make -C $$i depend; done

depend:
	for i in $(DIRS); do make -C $$i depend; done

clean:
	rm -f *.a
	for i in $(DIRS); do make -C $$i clean; done
	rm -rf *.o
