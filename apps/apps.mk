OBJ     =$(SRC:.c=.o)

all: $(OUTPUT)
	@objdump -Dx $(OUTPUT) > `basename $(OUTPUT) .exe`.dmp
	make install

-include ../../Makefile.inc ../Makefile.inc
-include ../../vars.mk ../vars.mk

$(OUTPUT): $(OBJ) $(PREFIX)/lib/libc.a $(PREFIX)/lib/libos.a
	$(PREFIX)/tools/makec.sh $(SRC) $(OUTPUT)

install: $(OUTPUT)
	$(PREFIX)/tools/copy_files.sh $(OUTPUT)

clean:
	for i in $(DIRS); do make -C $$i clean; done
	rm -f *.o
	rm -f *.dbg
	rm -f *.dmp
	rm -f $(OUTPUT)
