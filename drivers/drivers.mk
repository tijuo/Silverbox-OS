EXE     =$(SRC:.c=.exe)
INSTALL_DIR=servers/

all:    $(EXE)

-include Makefile.inc ../Makefile.inc ../../Makefile.inc
-include vars.mk ../vars.mk ../../vars.mk

%.exe:  %.c $(SB_PREFIX)/lib/libc.a $(SB_PREFIX)/lib/libos.a
	$(SB_PREFIX)/tools/makec.sh $< $@

install:$(EXE)
	$(SB_PREFIX)/tools/copy_files.sh $(EXE) -d $(INSTALL_DIR)

clean:
	rm -f *.o
	rm -f *.dmp
	rm -f *.dbg
	rm -f *.exe

