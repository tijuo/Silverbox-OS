include ../../Makefile.inc

ROOT_DIR	:=../..

ARFLAGS =rs

DIRS=ctype string stdlib errno time #stdio

.PHONY:	$(DIRS) all tests clean install

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

clean:
	rm -f *.a
	for i in $(DIRS); do make -C $$i clean; done
	rm -rf *.o

include ../../Makefile_post.inc
