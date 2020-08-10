OBJ     =$(SRC:.c=.o)
INSTALL_DIR=programs/

all: $(OUTPUT)
	@objdump -Dx $(OUTPUT) > `basename $(OUTPUT) .exe`.dmp
	make install

-include ../../Makefile.inc ../Makefile.inc
-include ../../vars.mk ../vars.mk

$(OUTPUT): $(OBJ) $(SB_PREFIX)/lib/libc.a $(SB_PREFIX)/lib/libos.a
	$(SB_PREFIX)/tools/makec.sh $(SRC) $(OUTPUT)

install: $(OUTPUT)
	$(SB_PREFIX)/tools/copy_files.sh $(OUTPUT) -d $(INSTALL_DIR)

clean:
	for i in $(DIRS); do make -C $$i clean; done
	rm -f *.o
	rm -f *.dbg
	rm -f *.dmp
	rm -f $(OUTPUT)
