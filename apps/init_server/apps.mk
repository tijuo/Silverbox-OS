OBJ     =$(SRC:.c=.o)
ASFLAGS	=-O0 -f elf
CFLAGS	=-O0 -Wall -m32 -std=gnu99 -fno-builtin -nostdlib -ffreestanding -nostartfiles -nodefaultlibs
STUBS	=$(addprefix $(PREFIX)/tools/,cstart.c _sig.c)
LDFLAGS	=-melf_i386 --exclude-libs ALL -T $(PREFIX)/cLink.ld --static

all: $(OUTPUT)
	@objdump -Dx $(OUTPUT) > `basename $(OUTPUT) .exe`.dmp
	make install

-include ../../vars.mk

libos.a: $(PREFIX)/lib/libos.a
	cp $(PREFIX)/lib/libos.a .
	ar -ds ./libos.a sbrk.o

$(OUTPUT): $(OBJ) $(PREFIX)/lib/libc.a libos.a
	gcc $(CFLAGS) -c -I $(PREFIX)/include $(SRC)
	gcc $(CFLAGS) -c -I $(PREFIX)/include $(STUBS)
	nasm $(ASFLAGS) $(PREFIX)/tools/_sig2.asm -o _sig2.o
	ld $(OBJ) cstart.o _sig.o _sig2.o -L$(PREFIX)/lib -\( -los -lc -\) $(LDFLAGS) -o $(OUTPUT)

#	$(PREFIX)/tools/makec.sh $(SRC) $(OUTPUT)

install: $(OUTPUT)
	$(PREFIX)/tools/copy_files.sh $(OUTPUT)

clean:
	rm -f *.o
	rm -f *.a
	rm -f *.dbg
	rm -f *.dmp
	rm -f $(OUTPUT)
