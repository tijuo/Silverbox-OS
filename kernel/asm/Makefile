SRC	=entry.asm gdt.asm idt.asm cpu_state.asm inthand.asm
OBJ	=$(SRC:.asm=.o)
DEPS	=$(INC_DIR)/asm/kernel.inc $(INC_DIR)/asm/grub.inc \
	$(INC_DIR)/asm/context.inc $(INC_DIR)/asm/asm.inc

CLEAN	=$(OBJ)

all:	$(OBJ)

include ../../vars.mk

%.o: %.asm $(DEPS)
	$(AS_I) $(ASIFLAGS) $< -o $@
