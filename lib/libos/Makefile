include ../../prefix.inc

SRC     =strings.c sbrk.c region.c bitmap.c elf.c \
	services.c

AS_SRC	=mutex.s state.s
AS_OBJ	=$(AS_SRC:.s=.o)
OBJ     =$(SRC:.c=.o)
DIRS	=syscall ostypes

.PHONY:	all syscall ostypes

all: libos.a libos_init.a

libos.a: $(OBJ) $(AS_OBJ)
	make -C ostypes/ all
	make -C syscall/ all
	for dir in $(DIRS); do make -C $$dir; done
	for dir in $(DIRS); do $(AR) $(ARFLAGS) $@ $$dir/*.o; done
	$(AR) $(ARFLAGS) $@ *.o

libos_init.a: libos.a
	cp libos.a libos_init.a
	ar -d libos_init.a sbrk.o

include ../../Makefile.inc

.c.o:
	$(CC) $(OPT_FLG) -c $(CFLAGS) -I$(INC_DIR) $< -o $@

.s.o:
	$(AS) $(AFLAGS) $< -o $@

depend:
	make -C ostypes/ depend
	make -C syscall/ depend
	$(DEPEND) -Y -I$(INC_DIR) -DDEBUG $(SRC)

clean:
	for dir in $(DIRS); do make -C $$dir clean; done
	rm -f *.o
# DO NOT DELETE

strings.o: /media/cb_Silverbox-OS/include/oslib.h
strings.o: /media/cb_Silverbox-OS/include/types.h
strings.o: /media/cb_Silverbox-OS/include/stddef.h
strings.o: /media/cb_Silverbox-OS/include/stdbool.h
strings.o: /media/cb_Silverbox-OS/include/limits.h
strings.o: /media/cb_Silverbox-OS/include/float.h
strings.o: /media/cb_Silverbox-OS/include/stdint.h
strings.o: /media/cb_Silverbox-OS/include/util.h
strings.o: /media/cb_Silverbox-OS/include/os/mutex.h
strings.o: /media/cb_Silverbox-OS/include/string.h
strings.o: /media/cb_Silverbox-OS/include/stdlib.h
strings.o: /media/cb_Silverbox-OS/include/stdio.h
strings.o: /media/cb_Silverbox-OS/include/stdarg.h
sbrk.o: /media/cb_Silverbox-OS/include/oslib.h
sbrk.o: /media/cb_Silverbox-OS/include/types.h
sbrk.o: /media/cb_Silverbox-OS/include/stddef.h
sbrk.o: /media/cb_Silverbox-OS/include/stdbool.h
sbrk.o: /media/cb_Silverbox-OS/include/limits.h
sbrk.o: /media/cb_Silverbox-OS/include/float.h
sbrk.o: /media/cb_Silverbox-OS/include/stdint.h
sbrk.o: /media/cb_Silverbox-OS/include/util.h
sbrk.o: /media/cb_Silverbox-OS/include/os/mutex.h
sbrk.o: /media/cb_Silverbox-OS/include/os/memory.h
sbrk.o: /media/cb_Silverbox-OS/include/os/services.h
sbrk.o: /media/cb_Silverbox-OS/include/os/device.h
sbrk.o: /media/cb_Silverbox-OS/include/os/region.h
sbrk.o: /media/cb_Silverbox-OS/include/os/vfs.h
sbrk.o: /media/cb_Silverbox-OS/include/os/file.h
sbrk.o: /media/cb_Silverbox-OS/include/os/os_types.h
sbrk.o: /media/cb_Silverbox-OS/include/os/ostypes/sbarray.h
sbrk.o: /media/cb_Silverbox-OS/include/os/ostypes/ostypes.h
sbrk.o: /media/cb_Silverbox-OS/include/os/ostypes/sbstring.h
sbrk.o: /media/cb_Silverbox-OS/include/stdlib.h
sbrk.o: /media/cb_Silverbox-OS/include/os/ostypes/sbhash.h
sbrk.o: /media/cb_Silverbox-OS/include/errno.h
region.o: /media/cb_Silverbox-OS/include/os/region.h
region.o: /media/cb_Silverbox-OS/include/stddef.h
region.o: /media/cb_Silverbox-OS/include/stdbool.h
region.o: /media/cb_Silverbox-OS/include/limits.h
region.o: /media/cb_Silverbox-OS/include/float.h
region.o: /media/cb_Silverbox-OS/include/types.h
region.o: /media/cb_Silverbox-OS/include/stdint.h
region.o: /media/cb_Silverbox-OS/include/util.h
bitmap.o: /media/cb_Silverbox-OS/include/os/bitmap.h
bitmap.o: /media/cb_Silverbox-OS/include/stddef.h
bitmap.o: /media/cb_Silverbox-OS/include/stdbool.h
bitmap.o: /media/cb_Silverbox-OS/include/limits.h
bitmap.o: /media/cb_Silverbox-OS/include/float.h
bitmap.o: /media/cb_Silverbox-OS/include/string.h
bitmap.o: /media/cb_Silverbox-OS/include/stdlib.h
elf.o: /media/cb_Silverbox-OS/include/types.h
elf.o: /media/cb_Silverbox-OS/include/stddef.h
elf.o: /media/cb_Silverbox-OS/include/stdbool.h
elf.o: /media/cb_Silverbox-OS/include/limits.h
elf.o: /media/cb_Silverbox-OS/include/float.h
elf.o: /media/cb_Silverbox-OS/include/stdint.h
elf.o: /media/cb_Silverbox-OS/include/util.h
elf.o: /media/cb_Silverbox-OS/include/elf.h
elf.o: /media/cb_Silverbox-OS/include/os/progload.h
services.o: /media/cb_Silverbox-OS/include/oslib.h
services.o: /media/cb_Silverbox-OS/include/types.h
services.o: /media/cb_Silverbox-OS/include/stddef.h
services.o: /media/cb_Silverbox-OS/include/stdbool.h
services.o: /media/cb_Silverbox-OS/include/limits.h
services.o: /media/cb_Silverbox-OS/include/float.h
services.o: /media/cb_Silverbox-OS/include/stdint.h
services.o: /media/cb_Silverbox-OS/include/util.h
services.o: /media/cb_Silverbox-OS/include/os/mutex.h
services.o: /media/cb_Silverbox-OS/include/os/services.h
services.o: /media/cb_Silverbox-OS/include/os/device.h
services.o: /media/cb_Silverbox-OS/include/os/region.h
services.o: /media/cb_Silverbox-OS/include/os/vfs.h
services.o: /media/cb_Silverbox-OS/include/os/file.h
services.o: /media/cb_Silverbox-OS/include/os/os_types.h
services.o: /media/cb_Silverbox-OS/include/os/ostypes/sbarray.h
services.o: /media/cb_Silverbox-OS/include/os/ostypes/ostypes.h
services.o: /media/cb_Silverbox-OS/include/os/ostypes/sbstring.h
services.o: /media/cb_Silverbox-OS/include/stdlib.h
services.o: /media/cb_Silverbox-OS/include/os/ostypes/sbhash.h
services.o: /media/cb_Silverbox-OS/include/os/message.h
services.o: /media/cb_Silverbox-OS/include/string.h
services.o: /media/cb_Silverbox-OS/include/os/dev_interface.h
services.o: /media/cb_Silverbox-OS/include/os/syscalls.h
services.o: /media/cb_Silverbox-OS/include/os/msg/message.h
services.o: /media/cb_Silverbox-OS/include/os/msg/init.h
services.o: /media/cb_Silverbox-OS/include/os/msg/rtc.h
