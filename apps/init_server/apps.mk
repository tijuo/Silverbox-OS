OBJ     =$(SRC:.c=.o) $(PREFIX)/tools/_sig.o $(PREFIX)/tools/_sig2.o start.o
ASFLAGS	=-O0 -f elf
CFLAGS	=-O0 -Wall -Wextra -m32 -std=gnu99 -fno-builtin -nostdlib -ffreestanding -nostartfiles -nodefaultlibs -fno-stack-unwind
#STUBS	=$(addprefix $(PREFIX)/tools/,cstart.c _sig.c)
LDFLAGS	=-melf_i386 --exclude-libs ALL -T $(PREFIX)/cLink.ld --static
INSTALL_DIR=sbos/servers/

all: $(OUTPUT)
	@objdump -Dx $(OUTPUT) > `basename $(OUTPUT) .exe`.dmp
	make install

-include ../../Makefile.inc
-include ../../vars.mk

libos.a: $(PREFIX)/lib/libos.a
	cp $(PREFIX)/lib/libos.a .
	ar -ds ./libos.a sbrk.o

start.o: start.asm
	nasm $(ASFLAGS) start.asm -o start.o

_sig.o: $(PREFIX)/tools/_sig.o
	gcc $(CFLAGS) -c -I $(PREFIX)/include $(PREFIX)/tools/_sig.c -o $(PREFIX)/tools/_sig.o
	cp $(PREFIX)/tools/_sig.o _sig.o

_sig2.o: $(PREFIX)/tools/_sig2.o
	nasm $(ASFLAGS) $(PREFIX)/tools/_sig2.asm -o $(PREFIX)/tools/_sig2.o

$(OUTPUT): $(OBJ) $(PREFIX)/lib/libc.a libos.a
	gcc $(CFLAGS) -c -I $(PREFIX)/include $(SRC)
#	gcc $(CFLAGS) -c -I $(PREFIX)/include $(STUBS)
	nasm $(ASFLAGS) $(PREFIX)/tools/_sig2.asm -o _sig2.o
	ld $(OBJ) -L$(PREFIX)/lib -\( -los -lc -\) $(LDFLAGS) -o $(OUTPUT)

#	$(PREFIX)/tools/makec.sh $(SRC) $(OUTPUT)

install: $(OUTPUT)
	$(PREFIX)/tools/copy_files.sh $(OUTPUT) -d $(INSTALL_DIR)

clean:
	rm -f *.o
	rm -f *.a
	rm -f *.dbg
	rm -f *.dmp
	rm -f $(OUTPUT)
