INC_DIR=$(SB_PREFIX)/include
CC	=i686-elf-gcc
AS	=i686-elf-as
AS_I	=nasm
LD	=i686-elf-ld
AR	=i686-elf-ar
AFLAGS	=--32
ASIFLAGS=-f elf32 -I$(INC_DIR)/
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
	-Wmissing-declarations -Wimplicit -Wimplicit-int \
	-Wno-address-of-packed-member -Wno-expansion-to-defined
DEF_PHONY=all clean depend dep install

#.SILENT :

.PHONY:	$(DEF_PHONY)
