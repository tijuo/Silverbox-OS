include ../Makefile.inc
include ../vars.mk

DIRS	=init_server shell

.PHONY:	$(DIRS) $(DEF_PHONY)

all:	$(DIRS)
	for i in $(DIRS); do make -C $$i all; done

init_server:
	make -C $@ all

shell:
	make -C $@ all

install: $(DIRS)

depend:
	for i in $(DIRS); do make -C $$i depend; done

clean:
	rm -f *.o
	rm -f *.dmp
	rm -f *.dbg
	rm -f *.exe
	for i in $(DIRS); do make -C $$i clean; done
