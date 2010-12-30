SRC	=
EXE	=

include ../drivers.mk

all:    $(SRC) $(EXE)
	$(PREFIX)/tools/makec.sh $(SRC) $(EXE)

#include ../vars.mk

install: $(EXE)
	$(PREFIX)/tools/copy_files.sh $(EXE)

clean:
	for i in $(DIRS); do make -C $$i clean; done
	rm -f *.o
	rm -f *.dmp
	rm -f *.dbg
	rm -f *.exe
