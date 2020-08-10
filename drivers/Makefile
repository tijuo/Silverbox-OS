include ../prefix.inc

SRC	=vidtest.c kbtest.c rdsktst.c memtst.c
EXE	=$(SRC:.c=.exe)
DIRS	=char block bus

.PHONY:	$(DIRS)

-include Makefile.inc ../Makefile.inc ../../Makefile.inc
-include vars.mk ../vars.mk ../../vars.mk

all:    $(DIRS) $(EXE)

%.exe:  %.c $(SB_PREFIX)/lib/libc.a $(SB_PREFIX)/lib/libos.a
	$(SB_PREFIX)/tools/makec.sh $< $@

char:
	make -C $@ all

block:
	make -C $@ all

bus:
	make -C $@ all

fs:
	make -C $@ all

install:
	for i in $(DIRS); do make -C $$i install; done
	$(SB_PREFIX)/tools/copy_files.sh $(EXE)

clean:
	for i in $(DIRS); do make -C $$i clean; done
	rm -f *.o
	rm -f *.dmp
	rm -f *.dbg
	rm -f *.exe
