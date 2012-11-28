# Set PREFIX to the path of the source dir (this one)

PREFIX	=$(HOME)/Silverbox-OS

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
CFLAGS  =-m32 -std=gnu99 -static -nostdlib \
        -fno-leading-underscore -nostartfiles \
        -ffreestanding -fno-exceptions -nodefaultlibs \
        -fno-stack-protector -fno-unwind-tables \
	-fno-non-call-exceptions -fno-stack-check \
	-fno-asynchronous-unwind-tables \
	-I$(INC_DIR) $(CMOREFLG) $(EN_DEBUG)
CMOREFLG=-Wall -Wextra -Wshadow -Wpointer-arith -Wsign-compare \
	-Wsystem-headers -Wstrict-prototypes \
	-Wunused-function -Wmissing-prototypes \
	-Wmissing-declarations -Wimplicit -Wimplicit-int

DEF_PHONY=all clean depend dep install

#.SILENT :

.PHONY:	$(DEF_PHONY)
