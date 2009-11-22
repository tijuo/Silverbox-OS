<<<<<<< HEAD:apps/init_server/Makefile
SRC     =list.c addr_space.c phys_alloc.c pagersrv.c shmem.c sbrk.c name.c
=======
SRC     =list.c addr_space.c phys_alloc.c pagersrv.c shmem.c name.c \
	exceptions.c sbrk.c
>>>>>>> 7ee7a89... Rearranged the memory map. Refactored initial server code.:apps/init_server/Makefile
OUTPUT	=pagersrv.exe

include ../apps.mk

# DO NOT DELETE

list.o: list.h /home/tiju/silverbox-os/include/stddef.h
list.o: /home/tiju/silverbox-os/include/stdbool.h
addr_space.o: addr_space.h /home/tiju/silverbox-os/include/os/bitmap.h
addr_space.o: /home/tiju/silverbox-os/include/stddef.h list.h
addr_space.o: /home/tiju/silverbox-os/include/stdbool.h phys_alloc.h
addr_space.o: pagersrv.h /home/tiju/silverbox-os/include/types.h
addr_space.o: /home/tiju/silverbox-os/include/os/region.h
phys_alloc.o: phys_alloc.h pagersrv.h /home/tiju/silverbox-os/include/types.h
phys_alloc.o: /home/tiju/silverbox-os/include/stddef.h
phys_alloc.o: /home/tiju/silverbox-os/include/stdbool.h list.h
phys_alloc.o: /home/tiju/silverbox-os/include/string.h
pagersrv.o: /home/tiju/silverbox-os/include/os/signal.h
pagersrv.o: /home/tiju/silverbox-os/include/oslib.h
pagersrv.o: /home/tiju/silverbox-os/include/types.h
pagersrv.o: /home/tiju/silverbox-os/include/stddef.h
pagersrv.o: /home/tiju/silverbox-os/include/stdbool.h
pagersrv.o: /home/tiju/silverbox-os/include/ipc.h
pagersrv.o: /home/tiju/silverbox-os/include/os/mutex.h
pagersrv.o: /home/tiju/silverbox-os/include/elf.h
pagersrv.o: /home/tiju/silverbox-os/include/string.h
pagersrv.o: /home/tiju/silverbox-os/include/stdlib.h
pagersrv.o: /home/tiju/silverbox-os/include/os/services.h
pagersrv.o: /home/tiju/silverbox-os/include/os/device.h
pagersrv.o: /home/tiju/silverbox-os/include/os/region.h
pagersrv.o: /home/tiju/silverbox-os/include/os/uthreads.h
pagersrv.o: /home/tiju/silverbox-os/include/os/message.h
pagersrv.o: /home/tiju/silverbox-os/include/os/dev_interface.h shmem.h
pagersrv.o: addr_space.h /home/tiju/silverbox-os/include/os/bitmap.h list.h
pagersrv.o: phys_alloc.h pagersrv.h
shmem.o: list.h /home/tiju/silverbox-os/include/stddef.h
shmem.o: /home/tiju/silverbox-os/include/stdbool.h phys_alloc.h pagersrv.h
shmem.o: /home/tiju/silverbox-os/include/types.h shmem.h addr_space.h
shmem.o: /home/tiju/silverbox-os/include/os/bitmap.h
shmem.o: /home/tiju/silverbox-os/include/os/region.h
shmem.o: /home/tiju/silverbox-os/include/oslib.h
shmem.o: /home/tiju/silverbox-os/include/ipc.h
shmem.o: /home/tiju/silverbox-os/include/os/mutex.h
shmem.o: /home/tiju/silverbox-os/include/stdlib.h
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
name.o: /home/tiju/silverbox-os/include/oslib.h
name.o: /home/tiju/silverbox-os/include/types.h
name.o: /home/tiju/silverbox-os/include/stddef.h
name.o: /home/tiju/silverbox-os/include/stdbool.h
name.o: /home/tiju/silverbox-os/include/ipc.h
name.o: /home/tiju/silverbox-os/include/os/mutex.h
name.o: /home/tiju/silverbox-os/include/os/services.h
name.o: /home/tiju/silverbox-os/include/os/device.h
name.o: /home/tiju/silverbox-os/include/os/region.h
name.o: /home/tiju/silverbox-os/include/string.h
