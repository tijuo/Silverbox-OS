EXE     =$(SRC:.c=.exe)
INSTALL_DIR=sbos/drivers/

-include Makefile.inc ../Makefile.inc ../../Makefile.inc

all:    $(EXE)

%.exe: %.c $(SB_PREFIX)/lib/libc/libc.a $(SB_PREFIX)/lib/libos/libos.a
	$(SB_PREFIX)/tools/makec.sh $< $@
	@objdump -dx $@ > `basename $@ .exe`.dmp

install: $(EXE)
	$(SB_PREFIX)/tools/copy_files.sh $(EXE) -d $(INSTALL_DIR)

clean:
	rm -f *.o
	rm -f *.dmp
	rm -f *.dbg
	rm -f *.exe

-include vars.mk ../vars.mk ../../vars.mk
