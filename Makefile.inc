# Set PREFIX to the path of the source dir (this one)

PREFIX	=/home/tiju/Silverbox-OS

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
EN_DEBUG=-DDEBUG
CFLAGS  =-m32 -std=gnu99 -fno-builtin -nostdlib \
        -fno-leading-underscore -nostartfiles \
        -ffreestanding -falign-functions \
        -fno-optimize-sibling-calls -fno-stack-protector \
        -I$(INC_DIR) $(CMOREFLG) $(EN_DEBUG)
CMOREFLG=-Wshadow -Wpointer-arith -Wsign-compare \
	-Wsystem-headers -Wstrict-prototypes \
	-Wunused-function -Wmissing-prototypes \
	-Wmissing-declarations

DEF_PHONY=all clean depend dep install

#.SILENT :

.PHONY:	$(DEF_PHONY)
