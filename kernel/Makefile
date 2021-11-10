include ../Makefile.inc

.PHONY:	all clean tests

ROOT_PATH	:=..
SRC	:=list.c message.c pic.c syscall.c debug.c \
    	interrupt.c mem.c paging.c schedule.c thread.c \
	init/init.c
ASM_SRC	=init/entry.S
OBJ	:=$(SRC:%.c=%.o) $(ASM_SRC:%.S=%.o)
BIN	:=kernel.elf

all: $(BIN)

$(BIN): $(OBJ)
	$(LD) $(LDFLAGS) -T kernelLink.ld $^ -o $@

tests:

clean:
	rm -f $(OBJ)
	rm -f $(BIN)
