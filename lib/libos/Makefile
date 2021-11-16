include ../../Makefile.inc

ROOT_DIR	:=../..

ARFLAGS =rs

DIRS	=ostypes
SRC	=bitmap.c elf.c sbrk.c strings.c message.c
ASM_SRC	=mutex.S state.S
OBJ	=$(SRC:%.c=%.o) $(ASM_SRC:%.S=%.o)

.PHONY:	$(DIRS) all tests clean install

all: libos.a

ostypes:
	make -C ostypes all

install:

libos.a: $(DIRS) $(OBJ)
	for dir in $(DIRS); do $(AR) $(ARFLAGS) $@ $$dir/*.o; done
	for obj in $(OBJ); do $(AR) $(ARFLAGS) $@ $$obj; done

libos_init.a: libos.a
	cp libos.a libos_init.a
	ar -d libos_init.a sbrk.o

clean:
	rm -f *.a
	for i in $(DIRS); do make -C $$i clean; done
	rm -rf *.o

include ../../Makefile_post.inc
