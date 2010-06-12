ARFLAGS =rs

LIBCDIRS=ctype string stdlib errno stdio time
LIBOSDIR=syscall oslib

.PHONY:	$(LIBCDIRS) $(LIBOSDIR) all dep depend clean

all:
	make libc.a
	make libos.a

ctype:
	make -C $@ all

string:
	make -C $@ all

oslib:
	make -C $@ all

syscall:
	make -C $@ all

stdlib:
	make -C $@ all

errno:
	make -C $@ all

stdio:
	make -C $@ all

time:
	make -C $@ all

libc.a: $(LIBCDIRS)
	for dir in $(LIBCDIRS); do $(AR) $(ARFLAGS) $@ $$dir/*.o; done

libos.a:$(LIBOSDIR)
	for dir in $(LIBOSDIR); do $(AR) $(ARFLAGS) $@ $$dir/*.o; done

dep:
	make depend

depend:
	for i in $(LIBOSDIR) $(LIBCDIRS); do make -C $$i depend; done

clean:
	rm -f *.a
	for i in $(LIBOSDIR) $(LIBCDIRS); do make -C $$i clean; done
	rm -rf *.o
