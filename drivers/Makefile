SRC	=vidtest.c kbtest.c rdsktst.c memtst.c
EXE	=$(SRC:.c=.exe)
DIRS	=char block bus

.PHONY:	$(DIRS)

-include Makefile.inc ../Makefile.inc ../../Makefile.inc
-include vars.mk ../vars.mk ../../vars.mk

all:    $(DIRS) $(EXE)

%.exe:  %.c $(PREFIX)/lib/libc.a $(PREFIX)/lib/libos.a
	$(PREFIX)/tools/makec.sh $< $@

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
	$(PREFIX)/tools/copy_files.sh $(EXE)

clean:
	for i in $(DIRS); do make -C $$i clean; done
	rm -f *.o
	rm -f *.dmp
	rm -f *.dbg
	rm -f *.exe
