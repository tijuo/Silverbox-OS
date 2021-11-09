include ../../prefix.inc
include ../../Makefile.inc

ARFLAGS =rs

DIRS=ctype string stdlib errno stdio time

.PHONY:	$(DIRS) all dep depend clean

all: libc.a

ctype:
	make -C $@ all

string:
	make -C $@ all

stdlib:
	make -C $@ all

errno:
	make -C $@ all

stdio:
	make -C $@ all

time:
	make -C $@ all

install:

libc.a: $(DIRS)
	for dir in $(DIRS); do $(AR) $(ARFLAGS) $@ $$dir/*.o; done

dep:
	make depend

depend:
	for i in $(DIRS); do make -C $$i depend; done

clean:
	rm -f *.a
	for i in $(DIRS); do make -C $$i clean; done
	rm -rf *.o
