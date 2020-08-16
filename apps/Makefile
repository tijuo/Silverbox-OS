include ../prefix.inc

DIRS	=init sender receiver rtc

.PHONY:	$(DIRS) $(DEF_PHONY)

all:	$(DIRS)
	for i in $(DIRS); do make -C $$i all; done

init_server:
	make -C $@ all

shell:
	make -C $@ all

install: $(DIRS)
	for i in $(DIRS); do make -C $$i install; done

depend:
	for i in $(DIRS); do make -C $$i depend; done

clean:
	rm -f *.o
	rm -f *.dmp
	rm -f *.dbg
	rm -f *.exe
	for i in $(DIRS); do make -C $$i clean; done

include ../Makefile.inc
include ../vars.mk

