INC_DIR=$(SB_PREFIX)/include
CC	=i686-elf-gcc
CPP	=i686-elf-g++
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
CPPFLAGS=-m32 -std=gnu++11 -static \
	-fno-leading-underscore -nostartfiles \
	-fno-exceptions -I/home/tijuo/src/gcc/libstdc++-v3/include/backward \
	-I/home/tijuo/src/gcc/libstdc++-v3/include/bits \
	-I/home/tijuo/src/gcc/libstdc++-v3/include/c \
	-I/home/tijuo/src/gcc/libstdc++-v3/include/c_compatibility \
	-I/home/tijuo/src/gcc/libstdc++-v3/include/c_global \
	-I/home/tijuo/src/gcc/libstdc++-v3/include/c_std \
	-I/home/tijuo/src/gcc/libstdc++-v3/include/debug \
	-I/home/tijuo/src/gcc/libstdc++-v3/include/decimal \
	-I/home/tijuo/src/gcc/libstdc++-v3/include/experimental \
	-I/home/tijuo/src/gcc/libstdc++-v3/include/ext \
	-I/home/tijuo/src/gcc/libstdc++-v3/include/parallel \
	-I/home/tijuo/src/gcc/libstdc++-v3/include/precompiled \
	-I/home/tijuo/src/gcc/libstdc++-v3/include/pstl \
	-I/home/tijuo/src/gcc/libstdc++-v3/include/std \
	-I/home/tijuo/src/gcc/libstdc++-v3/include/tr1 \
	-I/home/tijuo/src/gcc/libstdc++-v3/include/tr2 \
	-fno-rtti -Wall -Wextra -I$(INC_DIR)
CMOREFLG=-Wall -Wextra -Wshadow -Wpointer-arith -Wsign-compare \
	-Wsystem-headers -Wstrict-prototypes \
	-Wunused-function -Wmissing-prototypes \
	-Wmissing-declarations -Wimplicit -Wimplicit-int \
	-Wno-address-of-packed-member -Wno-expansion-to-defined
DEF_PHONY=all clean depend dep install

#.SILENT :

.PHONY:	$(DEF_PHONY)
