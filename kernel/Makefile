ROOT_PATH	:=..
include $(ROOT_PATH)/Makefile.inc

.PHONY:	all check clean tests install

SRC			=list.c message.c syscall.c debug.c \
    		interrupt.c mem.c paging.c schedule.c thread.c \
			apic.c init/init.c init/acpi.c init/loader.c init/libc.c \
			init/memory.c
ASM_SRC		=entry.S
OBJ			:=$(SRC:%.c=%.o) $(ASM_SRC:%.S=%.o)
BIN			:=kernel.elf
BIN_GZ		:=kernel.gz
CFLAGS  	:=$(CFLAGS) -mpreferred-stack-boundary=4

all: $(BIN_GZ) $(BIN).dmp $(BIN).sym $(BIN).bsym $(BIN).globals.dmp

$(BIN): $(OBJ) link.ld
	$(LD) $(LDFLAGS) -T link.ld $(OBJ) -lgcc -o $@

$(BIN_GZ): $(BIN)
	gzip -k $<
	mv $(BIN).gz $@

check:
	splint +gnuextensions -nolib -standard \
	-I$(ROOT_DIR)/include #-I$(CHECK_INC)/include \
#	-I$(CHECK_INC)/include-fixed *.c

install: $(BIN) $(BIN).dmp $(BIN).sym $(BIN).bsym $(BIN).globals.dmp
	$(ROOT_DIR)/tools/install_kernel.sh -d /boot kernel.elf
	
tests:

clean:
	rm -f $(OBJ)
	rm -f $(BIN)
	rm -f $(BIN).dmp
	rm -f $(BIN).gz
	rm -f $(BIN_GZ)
	rm -f $(BIN).sym
	rm -f $(BIN).bsym
	rm -f $(BIN).globals.dmp

include ../Makefile_post.inc
