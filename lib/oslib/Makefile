SRC     =strings.c sbrk.c region.c bitmap.c elf.c \
	services.c message.c driver.c console.c

AS_SRC	=mutex.s state.s
AS_OBJ	=$(AS_SRC:.s=.o)
OBJ     =$(SRC:.c=.o) $(AS_OBJ)

.PHONY:	all ostypes

all: $(OBJ)
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

strings.o: /home/tiju/Silverbox-OS/include/oslib.h
strings.o: /home/tiju/Silverbox-OS/include/types.h
strings.o: /home/tiju/Silverbox-OS/include/stddef.h
strings.o: /home/tiju/Silverbox-OS/include/stdbool.h
strings.o: /home/tiju/Silverbox-OS/include/ipc.h
strings.o: /home/tiju/Silverbox-OS/include/os/mutex.h
strings.o: /home/tiju/Silverbox-OS/include/string.h
sbrk.o: /home/tiju/Silverbox-OS/include/oslib.h
sbrk.o: /home/tiju/Silverbox-OS/include/types.h
sbrk.o: /home/tiju/Silverbox-OS/include/stddef.h
sbrk.o: /home/tiju/Silverbox-OS/include/stdbool.h
sbrk.o: /home/tiju/Silverbox-OS/include/ipc.h
sbrk.o: /home/tiju/Silverbox-OS/include/os/mutex.h
sbrk.o: /home/tiju/Silverbox-OS/include/os/memory.h
sbrk.o: /home/tiju/Silverbox-OS/include/os/services.h
sbrk.o: /home/tiju/Silverbox-OS/include/os/device.h
sbrk.o: /home/tiju/Silverbox-OS/include/os/region.h
sbrk.o: /home/tiju/Silverbox-OS/include/os/vfs.h
sbrk.o: /home/tiju/Silverbox-OS/include/os/file.h
sbrk.o: /home/tiju/Silverbox-OS/include/os/os_types.h
sbrk.o: /home/tiju/Silverbox-OS/include/os/ostypes/sbarray.h
sbrk.o: /home/tiju/Silverbox-OS/include/os/ostypes/sbstring.h
sbrk.o: /home/tiju/Silverbox-OS/include/stdlib.h
sbrk.o: /home/tiju/Silverbox-OS/include/os/ostypes/sbassocarray.h
region.o: /home/tiju/Silverbox-OS/include/os/region.h
region.o: /home/tiju/Silverbox-OS/include/stddef.h
region.o: /home/tiju/Silverbox-OS/include/stdbool.h
bitmap.o: /home/tiju/Silverbox-OS/include/os/bitmap.h
bitmap.o: /home/tiju/Silverbox-OS/include/stddef.h
bitmap.o: /home/tiju/Silverbox-OS/include/string.h
elf.o: /home/tiju/Silverbox-OS/include/types.h
elf.o: /home/tiju/Silverbox-OS/include/stddef.h
elf.o: /home/tiju/Silverbox-OS/include/stdbool.h
elf.o: /home/tiju/Silverbox-OS/include/elf.h
elf.o: /home/tiju/Silverbox-OS/include/os/progload.h
services.o: /home/tiju/Silverbox-OS/include/oslib.h
services.o: /home/tiju/Silverbox-OS/include/types.h
services.o: /home/tiju/Silverbox-OS/include/stddef.h
services.o: /home/tiju/Silverbox-OS/include/stdbool.h
services.o: /home/tiju/Silverbox-OS/include/ipc.h
services.o: /home/tiju/Silverbox-OS/include/os/mutex.h
services.o: /home/tiju/Silverbox-OS/include/os/services.h
services.o: /home/tiju/Silverbox-OS/include/os/device.h
services.o: /home/tiju/Silverbox-OS/include/os/region.h
services.o: /home/tiju/Silverbox-OS/include/os/vfs.h
services.o: /home/tiju/Silverbox-OS/include/os/file.h
services.o: /home/tiju/Silverbox-OS/include/os/os_types.h
services.o: /home/tiju/Silverbox-OS/include/os/ostypes/sbarray.h
services.o: /home/tiju/Silverbox-OS/include/os/ostypes/sbstring.h
services.o: /home/tiju/Silverbox-OS/include/stdlib.h
services.o: /home/tiju/Silverbox-OS/include/os/ostypes/sbassocarray.h
services.o: /home/tiju/Silverbox-OS/include/os/message.h
services.o: /home/tiju/Silverbox-OS/include/string.h
services.o: /home/tiju/Silverbox-OS/include/os/dev_interface.h
message.o: /home/tiju/Silverbox-OS/include/oslib.h
message.o: /home/tiju/Silverbox-OS/include/types.h
message.o: /home/tiju/Silverbox-OS/include/stddef.h
message.o: /home/tiju/Silverbox-OS/include/stdbool.h
message.o: /home/tiju/Silverbox-OS/include/ipc.h
message.o: /home/tiju/Silverbox-OS/include/os/mutex.h
message.o: /home/tiju/Silverbox-OS/include/os/message.h
message.o: /home/tiju/Silverbox-OS/include/string.h
message.o: /home/tiju/Silverbox-OS/include/os/io.h
driver.o: /home/tiju/Silverbox-OS/include/oslib.h
driver.o: /home/tiju/Silverbox-OS/include/types.h
driver.o: /home/tiju/Silverbox-OS/include/stddef.h
driver.o: /home/tiju/Silverbox-OS/include/stdbool.h
driver.o: /home/tiju/Silverbox-OS/include/ipc.h
driver.o: /home/tiju/Silverbox-OS/include/os/mutex.h
driver.o: /home/tiju/Silverbox-OS/include/os/dev_interface.h
driver.o: /home/tiju/Silverbox-OS/include/string.h
driver.o: /home/tiju/Silverbox-OS/include/os/message.h
console.o: /home/tiju/Silverbox-OS/include/oslib.h
console.o: /home/tiju/Silverbox-OS/include/types.h
console.o: /home/tiju/Silverbox-OS/include/stddef.h
console.o: /home/tiju/Silverbox-OS/include/stdbool.h
console.o: /home/tiju/Silverbox-OS/include/ipc.h
console.o: /home/tiju/Silverbox-OS/include/os/mutex.h
console.o: /home/tiju/Silverbox-OS/include/string.h
console.o: /home/tiju/Silverbox-OS/include/os/keys.h
console.o: /home/tiju/Silverbox-OS/include/os/dev_interface.h
console.o: /home/tiju/Silverbox-OS/include/os/services.h
console.o: /home/tiju/Silverbox-OS/include/os/device.h
console.o: /home/tiju/Silverbox-OS/include/os/region.h
console.o: /home/tiju/Silverbox-OS/include/os/vfs.h
console.o: /home/tiju/Silverbox-OS/include/os/file.h
console.o: /home/tiju/Silverbox-OS/include/os/os_types.h
console.o: /home/tiju/Silverbox-OS/include/os/ostypes/sbarray.h
console.o: /home/tiju/Silverbox-OS/include/os/ostypes/sbstring.h
console.o: /home/tiju/Silverbox-OS/include/stdlib.h
console.o: /home/tiju/Silverbox-OS/include/os/ostypes/sbassocarray.h
console.o: /home/tiju/Silverbox-OS/include/os/console.h
