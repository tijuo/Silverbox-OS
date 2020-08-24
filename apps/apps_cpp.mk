include ../../prefix.inc

OBJ     =$(SRC:.cc=.o)
INSTALL_DIR=sbos/programs/

all: $(OUTPUT)
	@objdump -Dx $(OUTPUT) > `basename $(OUTPUT) .exe`.dmp
	make install

$(OUTPUT): $(OBJ) $(SB_PREFIX)/lib/libc/libc.a $(SB_PREFIX)/lib/libos/libos.a
	$(SB_PREFIX)/tools/makecpp.sh $(SRC) $(OUTPUT)

install: $(OUTPUT)
	@objdump -Dx $(OUTPUT) > `basename $(OUTPUT) .exe`.dmp
	$(SB_PREFIX)/tools/copy_files.sh $(OUTPUT) -d $(INSTALL_DIR)

clean:
	for i in $(DIRS); do make -C $$i clean; done
	rm -f *.o
	rm -f *.dbg
	rm -f *.dmp
	rm -f $(OUTPUT)

-include ../../Makefile.inc ../Makefile.inc
-include ../../vars.mk ../vars.mk

