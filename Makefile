.PHONY:  kernel lib drivers apps all clean install depend dep

DIRS	=lib apps drivers kernel

all:	$(DIRS)

apps:
	make -C $@ all

kernel:
	make -C $@ all
	@cp kernel/kernel* bin/
	@rm -f bin/kernel.tmp

kernel-docs:
	doxygen kernel.cfg

lib:
	make -C $@ all

drivers:
	make -C $@ all

dep:
	make depend

depend:
	for i in $(DIRS); do make -C $$i depend; done

install: $(DIRS)
	for i in $(DIRS); do make -C $$i install; done

clean:
	for i in $(DIRS); do make -C $$i clean; done
	cd tools && rm -f *.o
	rm -rf doc/
