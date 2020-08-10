include ../../prefix.inc

OBJ     =$(SRC:.c=.o) start.o
ASFLAGS	=-O0 -f elf
CFLAGS	=-O0 -Wall -Wextra -m32 -std=gnu99 -fno-builtin -nostdlib -ffreestanding -nostartfiles -nodefaultlibs -fno-stack-unwind
#STUBS	=$(addprefix $(SB_PREFIX)/tools/,cstart.c)
LDFLAGS	=-melf_i386 --exclude-libs ALL -T $(SB_PREFIX)/cLink.ld --static
INSTALL_DIR=sbos/servers/

all: $(OUTPUT)
	@objdump -Dx $(OUTPUT) > `basename $(OUTPUT) .exe`.dmp
	make install

-include ../../Makefile.inc
-include ../../vars.mk

libos.a: $(SB_PREFIX)/lib/libos.a
	cp $(SB_PREFIX)/lib/libos.a .
	ar -ds ./libos.a sbrk.o

start.o: start.asm
	nasm $(ASFLAGS) start.asm -o start.o

$(OUTPUT): $(OBJ) $(SB_PREFIX)/lib/libc.a libos.a
	gcc $(CFLAGS) -c -I $(SB_PREFIX)/include $(SRC)
#	gcc $(CFLAGS) -c -I $(SB_PREFIX)/include $(STUBS)
	ld $(OBJ) -L$(SB_PREFIX)/lib -\( -los -lc -\) $(LDFLAGS) -o $(OUTPUT)

#	$(SB_PREFIX)/tools/makec.sh $(SRC) $(OUTPUT)

install: $(OUTPUT)
	$(SB_PREFIX)/tools/copy_files.sh $(OUTPUT) -d $(INSTALL_DIR)

clean:
	rm -f *.o
	rm -f *.a
	rm -f *.dbg
	rm -f *.dmp
	rm -f $(OUTPUT)
