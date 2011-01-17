SRC	=vidtest.c kbtest.c rdsktst.c memtst.c
EXE	=$(SRC:.c=.exe)
DIRS	=char block bus

.PHONY:	$(DIRS)

-include Makefile.inc ../Makefile.inc ../../Makefile.inc
-include vars.mk ../vars.mk ../../vars.mk

all:    $(DIRS) $(EXE)

%.exe:  %.c $(PREFIX)/lib/libc.a $(PREFIX)/lib/libos.a
	$(PREFIX)/tools/makec.sh $< $@

char:
	make -C $@ all

block:
	make -C $@ all

bus:
	make -C $@ all

fs:
	make -C $@ all

install:
	for i in $(DIRS); do make -C $$i install; done
	$(PREFIX)/tools/copy_files.sh $(EXE)

clean:
	for i in $(DIRS); do make -C $$i clean; done
	rm -f *.o
	rm -f *.dmp
	rm -f *.dbg
	rm -f *.exe
# DO NOT DELETE

vidtest.o: /home/tiju/Silverbox-OS/include/oslib.h
vidtest.o: /home/tiju/Silverbox-OS/include/types.h
vidtest.o: /home/tiju/Silverbox-OS/include/stddef.h
vidtest.o: /home/tiju/Silverbox-OS/include/stdbool.h
vidtest.o: /home/tiju/Silverbox-OS/include/ipc.h
vidtest.o: /home/tiju/Silverbox-OS/include/os/mutex.h
vidtest.o: /home/tiju/Silverbox-OS/include/string.h
vidtest.o: /home/tiju/Silverbox-OS/include/os/dev_interface.h
vidtest.o: /home/tiju/Silverbox-OS/include/os/services.h
vidtest.o: /home/tiju/Silverbox-OS/include/os/device.h
vidtest.o: /home/tiju/Silverbox-OS/include/os/region.h
vidtest.o: /home/tiju/Silverbox-OS/include/os/console.h
kbtest.o: /home/tiju/Silverbox-OS/include/oslib.h
kbtest.o: /home/tiju/Silverbox-OS/include/types.h
kbtest.o: /home/tiju/Silverbox-OS/include/stddef.h
kbtest.o: /home/tiju/Silverbox-OS/include/stdbool.h
kbtest.o: /home/tiju/Silverbox-OS/include/ipc.h
kbtest.o: /home/tiju/Silverbox-OS/include/os/mutex.h
kbtest.o: /home/tiju/Silverbox-OS/include/string.h
kbtest.o: /home/tiju/Silverbox-OS/include/os/dev_interface.h
kbtest.o: /home/tiju/Silverbox-OS/include/os/services.h
kbtest.o: /home/tiju/Silverbox-OS/include/os/device.h
kbtest.o: /home/tiju/Silverbox-OS/include/os/region.h
kbtest.o: /home/tiju/Silverbox-OS/include/os/keys.h
kbtest.o: /home/tiju/Silverbox-OS/include/os/console.h
rdsktst.o: /home/tiju/Silverbox-OS/include/os/console.h
rdsktst.o: /home/tiju/Silverbox-OS/include/types.h
rdsktst.o: /home/tiju/Silverbox-OS/include/stddef.h
rdsktst.o: /home/tiju/Silverbox-OS/include/stdbool.h
rdsktst.o: /home/tiju/Silverbox-OS/include/os/dev_interface.h
rdsktst.o: /home/tiju/Silverbox-OS/include/oslib.h
rdsktst.o: /home/tiju/Silverbox-OS/include/ipc.h
rdsktst.o: /home/tiju/Silverbox-OS/include/os/mutex.h
rdsktst.o: /home/tiju/Silverbox-OS/include/string.h
memtst.o: /home/tiju/Silverbox-OS/include/stdlib.h
memtst.o: /home/tiju/Silverbox-OS/include/stddef.h
memtst.o: /home/tiju/Silverbox-OS/include/oslib.h
memtst.o: /home/tiju/Silverbox-OS/include/types.h
memtst.o: /home/tiju/Silverbox-OS/include/stdbool.h
memtst.o: /home/tiju/Silverbox-OS/include/ipc.h
memtst.o: /home/tiju/Silverbox-OS/include/os/mutex.h
