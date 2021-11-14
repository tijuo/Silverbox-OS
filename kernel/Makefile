ROOT_PATH	:=..
include $(ROOT_PATH)/Makefile.inc

.PHONY:	all check clean tests install

SRC	:=list.c message.c pic.c syscall.c debug.c \
    	interrupt.c mem.c paging.c schedule.c thread.c \
	init.c
ASM_SRC	=entry.S
OBJ	:=$(SRC:%.c=%.o) $(ASM_SRC:%.S=%.o)
BIN	:=kernel.elf
BIN_GZ	:=kernel.gz

CHECK_INC=/opt/cross/i686-elf/lib/gcc/i686-elf/12.0.0

all: $(BIN_GZ) $(BIN).dmp $(BIN).sym $(BIN).bsym

$(BIN): $(OBJ)
	$(LD) $(LDFLAGS) -T kernelLink.ld $(OBJ) -o $@

$(BIN_GZ): $(BIN)
	gzip -k $<
	mv $(BIN).gz $@

check:
	splint +gnuextensions -nolib -standard \
	-I$(ROOT_DIR)/include -I$(CHECK_INC)/include \
	-I$(CHECK_INC)/include-fixed *.c

install: $(BIN_GZ)
	mcopy -D o -i $(FLOPPY_IMG) $< ::boot

tests:

clean:
	rm -f $(OBJ)
	rm -f $(BIN)
	rm -f $(BIN).dmp
	rm -f $(BIN).gz
	rm -f $(BIN_GZ)

include ../Makefile_post.inc
