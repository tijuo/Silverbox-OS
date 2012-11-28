EXE     =$(SRC:.c=.exe)
INSTALL_DIR=servers/

all:    $(EXE)

-include Makefile.inc ../Makefile.inc ../../Makefile.inc
-include vars.mk ../vars.mk ../../vars.mk

%.exe:  %.c $(PREFIX)/lib/libc.a $(PREFIX)/lib/libos.a
	$(PREFIX)/tools/makec.sh $< $@

install:$(EXE)
	$(PREFIX)/tools/copy_files.sh $(EXE) -d $(INSTALL_DIR)

clean:
	rm -f *.o
	rm -f *.dmp
	rm -f *.dbg
	rm -f *.exe

