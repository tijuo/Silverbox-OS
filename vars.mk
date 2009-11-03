# Set PREFIX to the path of the source dir (this one)

PREFIX	=/home/tiju/silverbox-os

INC_DIR	=$(PREFIX)/include
CC	=gcc
AS	=as
AS_I	=nasm
LD	=ld
AR	=ar
AFLAGS	=
ASIFLAGS=-f elf -I$(INC_DIR)/
DEPEND	=makedepend
DD	=dd
CFLAGS  =-m32 -std=gnu99 -fno-builtin -nostdlib \
        -fno-leading-underscore -nostartfiles \
        -ffreestanding -falign-functions \
        -fno-optimize-sibling-calls -fno-stack-protector \
        -I$(INC_DIR) $(CMOREFLG) -DDEBUG
CMOREFLG=-Wshadow -Wpointer-arith -Wsign-compare \
	-Wsystem-headers -Wstrict-prototypes \
	-Wunused-function -Wmissing-prototypes \
	-Wmissing-declarations
DEF_PHONY=all clean depend dep install

.PHONY:	$(DEF_PHONY)

.c.o:
	$(CC) $(OPT_FLG) -c $(CFLAGS) -I$(INC_DIR) $< -o $@

.s.o:
	$(AS) $(AFLAGS) $< -o $@

$(PREFIX)/lib/libc.a:
	make -C $(PREFIX)/lib/ libc.a

$(PREFIX)/lib/libos.a:
	make -C $(PREFIX)/lib/ libos.a

dep:
	make depend
depend:
	$(DEPEND) -Y -I$(INC_DIR) -DDEBUG $(SRC)

clean:
	rm -f $(CLEAN)
