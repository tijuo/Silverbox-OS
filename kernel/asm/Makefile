include ../../Makefile.inc

SRC	=entry.asm gdt.asm idt.asm cpu_state.asm inthand.asm io.asm \
	memory.asm
OBJ	=$(SRC:.asm=.o)
DEPS	=$(INC_DIR)/asm/kernel.inc $(INC_DIR)/asm/grub.inc \
	$(INC_DIR)/asm/context.inc $(INC_DIR)/asm/asm.inc

all:	$(OBJ)

clean:
	rm -f *.o

%.o: %.asm $(INC_DIR)
	$(AS_I) $(ASIFLAGS) $< -o $@
