SRC     =strings.c sbrk.c region.c bitmap.c elf.c \
	services.c

AS_SRC	=mutex.s state.s
AS_OBJ	=$(AS_SRC:.s=.o)
OBJ     =$(SRC:.c=.o)

.PHONY:	all ostypes

all: $(OBJ) $(AS_OBJ)
	make -C ostypes/ all
	@cp ostypes/*.o .

include ../../Makefile.inc

.c.o:
	$(CC) $(OPT_FLG) -c $(CFLAGS) -I$(INC_DIR) $< -o $@

.s.o:
	$(AS) $(AFLAGS) $< -o $@

depend:
	make -C ostypes/ depend
	$(DEPEND) -Y -I$(INC_DIR) -DDEBUG $(SRC)

clean:
	make -C ostypes/ clean
	rm -f *.o
