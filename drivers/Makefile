SRC	=vidtest.c kbtest.c rdsktst.c memtst.c read_fat.c
EXE	=$(SRC:.c=.exe)
DIRS	=char block bus

.PHONY:	$(DIRS)

include drivers.mk

all:    $(DIRS) $(EXE)

#include ../vars.mk

char:
	make -C $@ all

block:
	make -C $@ all

bus:
	make -C $@ all

install: $(DIRS) $(EXE)
	for i in $(DIRS); do make -C $$i install; done
	echo $(EXE)
	$(PREFIX)/tools/copy_files.sh $(EXE)

clean:
	for i in $(DIRS); do make -C $$i clean; done
	rm -f *.o
	rm -f *.dmp
	rm -f *.dbg
	rm -f *.exe
# DO NOT DELETE

vidtest.o: /home/tiju/silverbox-os/include/oslib.h
vidtest.o: /home/tiju/silverbox-os/include/types.h
vidtest.o: /home/tiju/silverbox-os/include/stddef.h
vidtest.o: /home/tiju/silverbox-os/include/stdbool.h
vidtest.o: /home/tiju/silverbox-os/include/ipc.h
vidtest.o: /home/tiju/silverbox-os/include/os/mutex.h
vidtest.o: /home/tiju/silverbox-os/include/string.h
vidtest.o: /home/tiju/silverbox-os/include/os/dev_interface.h
vidtest.o: /home/tiju/silverbox-os/include/os/services.h
vidtest.o: /home/tiju/silverbox-os/include/os/device.h
vidtest.o: /home/tiju/silverbox-os/include/os/region.h
vidtest.o: /home/tiju/silverbox-os/include/os/console.h
kbtest.o: /home/tiju/silverbox-os/include/oslib.h
kbtest.o: /home/tiju/silverbox-os/include/types.h
kbtest.o: /home/tiju/silverbox-os/include/stddef.h
kbtest.o: /home/tiju/silverbox-os/include/stdbool.h
kbtest.o: /home/tiju/silverbox-os/include/ipc.h
kbtest.o: /home/tiju/silverbox-os/include/os/mutex.h
kbtest.o: /home/tiju/silverbox-os/include/string.h
kbtest.o: /home/tiju/silverbox-os/include/os/dev_interface.h
kbtest.o: /home/tiju/silverbox-os/include/os/services.h
kbtest.o: /home/tiju/silverbox-os/include/os/device.h
kbtest.o: /home/tiju/silverbox-os/include/os/region.h
kbtest.o: /home/tiju/silverbox-os/include/os/keys.h
rdsktst.o: /home/tiju/silverbox-os/include/os/console.h
rdsktst.o: /home/tiju/silverbox-os/include/types.h
rdsktst.o: /home/tiju/silverbox-os/include/stddef.h
rdsktst.o: /home/tiju/silverbox-os/include/stdbool.h
rdsktst.o: /home/tiju/silverbox-os/include/os/dev_interface.h
rdsktst.o: /home/tiju/silverbox-os/include/oslib.h
rdsktst.o: /home/tiju/silverbox-os/include/ipc.h
rdsktst.o: /home/tiju/silverbox-os/include/os/mutex.h
rdsktst.o: /home/tiju/silverbox-os/include/string.h
memtst.o: /home/tiju/silverbox-os/include/stdlib.h
memtst.o: /home/tiju/silverbox-os/include/stddef.h
memtst.o: /home/tiju/silverbox-os/include/oslib.h
memtst.o: /home/tiju/silverbox-os/include/types.h
memtst.o: /home/tiju/silverbox-os/include/stdbool.h
memtst.o: /home/tiju/silverbox-os/include/ipc.h
memtst.o: /home/tiju/silverbox-os/include/os/mutex.h
read_fat.o: /home/tiju/silverbox-os/include/oslib.h
read_fat.o: /home/tiju/silverbox-os/include/types.h
read_fat.o: /home/tiju/silverbox-os/include/stddef.h
read_fat.o: /home/tiju/silverbox-os/include/stdbool.h
read_fat.o: /home/tiju/silverbox-os/include/ipc.h
read_fat.o: /home/tiju/silverbox-os/include/os/mutex.h
read_fat.o: /home/tiju/silverbox-os/include/os/vfs.h
read_fat.o: /home/tiju/silverbox-os/include/os/file.h
read_fat.o: /home/tiju/silverbox-os/include/string.h
read_fat.o: /home/tiju/silverbox-os/include/stdlib.h
read_fat.o: /home/tiju/silverbox-os/include/os/services.h
read_fat.o: /home/tiju/silverbox-os/include/os/device.h
read_fat.o: /home/tiju/silverbox-os/include/os/region.h
read_fat.o: /home/tiju/silverbox-os/include/os/dev_interface.h
read_fat.o: /home/tiju/silverbox-os/include/os/console.h
