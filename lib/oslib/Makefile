include ../../prefix.inc

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
# DO NOT DELETE

strings.o: /media/Silverbox-OS/include/oslib.h
strings.o: /media/Silverbox-OS/include/types.h
strings.o: /media/Silverbox-OS/include/stddef.h
strings.o: /media/Silverbox-OS/include/stdbool.h
strings.o: /media/Silverbox-OS/include/util.h
strings.o: /media/Silverbox-OS/include/ipc.h
strings.o: /media/Silverbox-OS/include/os/mutex.h
strings.o: /media/Silverbox-OS/include/string.h
strings.o: /media/Silverbox-OS/include/stdlib.h
sbrk.o: /media/Silverbox-OS/include/oslib.h
sbrk.o: /media/Silverbox-OS/include/types.h
sbrk.o: /media/Silverbox-OS/include/stddef.h
sbrk.o: /media/Silverbox-OS/include/stdbool.h
sbrk.o: /media/Silverbox-OS/include/util.h /media/Silverbox-OS/include/ipc.h
sbrk.o: /media/Silverbox-OS/include/os/mutex.h
sbrk.o: /media/Silverbox-OS/include/os/memory.h
sbrk.o: /media/Silverbox-OS/include/os/services.h
sbrk.o: /media/Silverbox-OS/include/os/device.h
sbrk.o: /media/Silverbox-OS/include/os/region.h
sbrk.o: /media/Silverbox-OS/include/os/vfs.h
sbrk.o: /media/Silverbox-OS/include/os/file.h
sbrk.o: /media/Silverbox-OS/include/os/os_types.h
sbrk.o: /media/Silverbox-OS/include/os/ostypes/sbarray.h
sbrk.o: /media/Silverbox-OS/include/os/ostypes/sbstring.h
sbrk.o: /media/Silverbox-OS/include/stdlib.h
sbrk.o: /media/Silverbox-OS/include/os/ostypes/sbassocarray.h
sbrk.o: /media/Silverbox-OS/include/errno.h
region.o: /media/Silverbox-OS/include/os/region.h
region.o: /media/Silverbox-OS/include/stddef.h
region.o: /media/Silverbox-OS/include/stdbool.h
region.o: /media/Silverbox-OS/include/types.h
region.o: /media/Silverbox-OS/include/util.h
bitmap.o: /media/Silverbox-OS/include/os/bitmap.h
bitmap.o: /media/Silverbox-OS/include/stddef.h
bitmap.o: /media/Silverbox-OS/include/string.h
elf.o: /media/Silverbox-OS/include/types.h
elf.o: /media/Silverbox-OS/include/stddef.h
elf.o: /media/Silverbox-OS/include/stdbool.h
elf.o: /media/Silverbox-OS/include/util.h /media/Silverbox-OS/include/elf.h
elf.o: /media/Silverbox-OS/include/os/progload.h
services.o: /media/Silverbox-OS/include/oslib.h
services.o: /media/Silverbox-OS/include/types.h
services.o: /media/Silverbox-OS/include/stddef.h
services.o: /media/Silverbox-OS/include/stdbool.h
services.o: /media/Silverbox-OS/include/util.h
services.o: /media/Silverbox-OS/include/ipc.h
services.o: /media/Silverbox-OS/include/os/mutex.h
services.o: /media/Silverbox-OS/include/os/services.h
services.o: /media/Silverbox-OS/include/os/device.h
services.o: /media/Silverbox-OS/include/os/region.h
services.o: /media/Silverbox-OS/include/os/vfs.h
services.o: /media/Silverbox-OS/include/os/file.h
services.o: /media/Silverbox-OS/include/os/os_types.h
services.o: /media/Silverbox-OS/include/os/ostypes/sbarray.h
services.o: /media/Silverbox-OS/include/os/ostypes/sbstring.h
services.o: /media/Silverbox-OS/include/stdlib.h
services.o: /media/Silverbox-OS/include/os/ostypes/sbassocarray.h
services.o: /media/Silverbox-OS/include/os/message.h
services.o: /media/Silverbox-OS/include/string.h
services.o: /media/Silverbox-OS/include/os/dev_interface.h
services.o: /media/Silverbox-OS/include/os/syscalls.h
