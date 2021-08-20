include ../../prefix.inc
include ../../Makefile.inc

#SRC	=entry.asm gdt.asm idt.asm cpu_state.asm inthand.asm io.asm
#OBJ	=$(SRC:.asm=.o)
#DEPS	=$(INC_DIR)/asm/kernel.inc $(INC_DIR)/asm/grub.inc \
#	$(INC_DIR)/asm/context.inc $(INC_DIR)/asm/asm.inc

SRC	=entry.S gdt.S idt.S cpu_state.S inthand.S io.S stack.S
OBJ	=$(SRC:.S=.o)
DEPS	=$(INC_DIR)/asm/kernel.h $(INC_DIR)/asm/grub.h \
	$(INC_DIR)/asm/context.h $(INC_DIR)/asm/asm.h

all:	$(OBJ)

clean:
	rm -f *.o

#%.o: %.asm $(INC_DIR)
#	$(AS_I) $(ASIFLAGS) $< -o $@

%.o: %.S $(INC_DIR)
	$(AS) $(AFLAGS) $< -o $@
