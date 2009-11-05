SRC     =strings.c sbrk.c region.c bitmap.c \
	services.c longmsg.c fat_lib.c fat_alloc.c \
	driver.c console.c

AS_SRC	=mutex.s state.s
AS_OBJ	=$(AS_SRC:.s=.o)
OBJ     =$(SRC:.c=.o) $(AS_OBJ)
OPT_FLG =-O0
CLEAN   =$(OBJ)

.PHONY:	adt

all: $(OBJ)
	make -C adt/ all
	@cp adt/*.o .

include ../../vars.mk

dep:
	make depend

depend:
	make -C adt/ depend
	$(DEPEND) -Y -I$(INC_DIR) -DDEBUG $(SRC)

clean:
	make -C adt/ clean
	rm -f *.o

# DO NOT DELETE

strings.o: /home/tiju/silverbox-os/include/oslib.h
strings.o: /home/tiju/silverbox-os/include/types.h
strings.o: /home/tiju/silverbox-os/include/stddef.h
strings.o: /home/tiju/silverbox-os/include/stdbool.h
strings.o: /home/tiju/silverbox-os/include/ipc.h
strings.o: /home/tiju/silverbox-os/include/os/mutex.h
sbrk.o: /home/tiju/silverbox-os/include/oslib.h
sbrk.o: /home/tiju/silverbox-os/include/types.h
sbrk.o: /home/tiju/silverbox-os/include/stddef.h
sbrk.o: /home/tiju/silverbox-os/include/stdbool.h
sbrk.o: /home/tiju/silverbox-os/include/ipc.h
sbrk.o: /home/tiju/silverbox-os/include/os/mutex.h
sbrk.o: /home/tiju/silverbox-os/include/os/memory.h
sbrk.o: /home/tiju/silverbox-os/include/os/services.h
sbrk.o: /home/tiju/silverbox-os/include/os/device.h
sbrk.o: /home/tiju/silverbox-os/include/os/region.h
region.o: /home/tiju/silverbox-os/include/os/region.h
region.o: /home/tiju/silverbox-os/include/stddef.h
region.o: /home/tiju/silverbox-os/include/stdbool.h
bitmap.o: /home/tiju/silverbox-os/include/os/bitmap.h
bitmap.o: /home/tiju/silverbox-os/include/stddef.h
bitmap.o: /home/tiju/silverbox-os/include/string.h
services.o: /home/tiju/silverbox-os/include/oslib.h
services.o: /home/tiju/silverbox-os/include/types.h
services.o: /home/tiju/silverbox-os/include/stddef.h
services.o: /home/tiju/silverbox-os/include/stdbool.h
services.o: /home/tiju/silverbox-os/include/ipc.h
services.o: /home/tiju/silverbox-os/include/os/mutex.h
services.o: /home/tiju/silverbox-os/include/os/services.h
services.o: /home/tiju/silverbox-os/include/os/device.h
services.o: /home/tiju/silverbox-os/include/os/region.h
services.o: /home/tiju/silverbox-os/include/os/message.h
services.o: /home/tiju/silverbox-os/include/string.h
services.o: /home/tiju/silverbox-os/include/os/dev_interface.h
longmsg.o: /home/tiju/silverbox-os/include/oslib.h
longmsg.o: /home/tiju/silverbox-os/include/types.h
longmsg.o: /home/tiju/silverbox-os/include/stddef.h
longmsg.o: /home/tiju/silverbox-os/include/stdbool.h
longmsg.o: /home/tiju/silverbox-os/include/ipc.h
longmsg.o: /home/tiju/silverbox-os/include/os/mutex.h
longmsg.o: /home/tiju/silverbox-os/include/os/message.h
longmsg.o: /home/tiju/silverbox-os/include/string.h
longmsg.o: /home/tiju/silverbox-os/include/os/io.h
fat_lib.o: /home/tiju/silverbox-os/include/os/fatfs.h
fat_lib.o: /home/tiju/silverbox-os/include/types.h
fat_lib.o: /home/tiju/silverbox-os/include/stddef.h
fat_lib.o: /home/tiju/silverbox-os/include/stdbool.h
fat_lib.o: /home/tiju/silverbox-os/include/os/device.h
fat_lib.o: /home/tiju/silverbox-os/include/oslib.h
fat_lib.o: /home/tiju/silverbox-os/include/ipc.h
fat_lib.o: /home/tiju/silverbox-os/include/os/mutex.h
fat_lib.o: /home/tiju/silverbox-os/include/string.h
fat_lib.o: /home/tiju/silverbox-os/include/stdlib.h
fat_lib.o: /home/tiju/silverbox-os/include/ctype.h
fat_lib.o: /home/tiju/silverbox-os/include/os/vfs.h
fat_lib.o: /home/tiju/silverbox-os/include/os/file.h
fat_lib.o: /home/tiju/silverbox-os/include/os/dev_interface.h
fat_lib.o: /home/tiju/silverbox-os/include/os/services.h
fat_lib.o: /home/tiju/silverbox-os/include/os/region.h
fat_alloc.o: /home/tiju/silverbox-os/include/os/fatfs.h
fat_alloc.o: /home/tiju/silverbox-os/include/types.h
fat_alloc.o: /home/tiju/silverbox-os/include/stddef.h
fat_alloc.o: /home/tiju/silverbox-os/include/stdbool.h
fat_alloc.o: /home/tiju/silverbox-os/include/os/device.h
fat_alloc.o: /home/tiju/silverbox-os/include/oslib.h
fat_alloc.o: /home/tiju/silverbox-os/include/ipc.h
fat_alloc.o: /home/tiju/silverbox-os/include/os/mutex.h
fat_alloc.o: /home/tiju/silverbox-os/include/string.h
fat_alloc.o: /home/tiju/silverbox-os/include/stdlib.h
driver.o: /home/tiju/silverbox-os/include/oslib.h
driver.o: /home/tiju/silverbox-os/include/types.h
driver.o: /home/tiju/silverbox-os/include/stddef.h
driver.o: /home/tiju/silverbox-os/include/stdbool.h
driver.o: /home/tiju/silverbox-os/include/ipc.h
driver.o: /home/tiju/silverbox-os/include/os/mutex.h
driver.o: /home/tiju/silverbox-os/include/os/dev_interface.h
driver.o: /home/tiju/silverbox-os/include/string.h
driver.o: /home/tiju/silverbox-os/include/os/message.h
console.o: /home/tiju/silverbox-os/include/oslib.h
console.o: /home/tiju/silverbox-os/include/types.h
console.o: /home/tiju/silverbox-os/include/stddef.h
console.o: /home/tiju/silverbox-os/include/stdbool.h
console.o: /home/tiju/silverbox-os/include/ipc.h
console.o: /home/tiju/silverbox-os/include/os/mutex.h
console.o: /home/tiju/silverbox-os/include/string.h
console.o: /home/tiju/silverbox-os/include/os/keys.h
console.o: /home/tiju/silverbox-os/include/os/dev_interface.h
console.o: /home/tiju/silverbox-os/include/os/services.h
console.o: /home/tiju/silverbox-os/include/os/device.h
console.o: /home/tiju/silverbox-os/include/os/region.h
console.o: /home/tiju/silverbox-os/include/os/console.h
