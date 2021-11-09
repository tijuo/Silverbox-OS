include ../Makefile.inc

SRC	:=init.c list.c message.c pic.c syscall.c debug.c \
    	interrupt.c mem.c paging.c schedule.c thread.c \
#	asm/entry.S asm/inthand.S asm/io.S
OBJ	:=$(SRC:%.c=%.o)
BIN	:=kernel.elf

all: all-debug

all-debug: set-debug-flags $(BIN)

all-release: set-release-flags $(BIN)

$(BIN): $(OBJ)
	$(LD) $(LDFLAGS) -T kernelLink.ld $^ -o $@

tests:

clean:
	rm -f $(OBJ)
	rm -f $(BIN)
