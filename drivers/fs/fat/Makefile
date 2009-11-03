INC_DIR =$(SILVERBOX)/include/ $(SILVERBOX)/drivers/include
ASFLAGS =-f elf
CC      =gcc
CPP     =g++
LD      =ld
AS      =nasm
AR      =ar
TMP     =../tmp

SRC     =ramdisk.c

all:
	$(SILVERBOX)/tools/makec.sh $$i $(SILVERBOX)/drivers/libdrv.c fat_lib.c fat_alloc.c fat.exe; \
	make update

update:
	$(SILVERBOX)/tools/copy_files.sh fat.exe
#.c.o:

clean:
	rm -f *.o
	rm -f *.dmp
	rm -f *.dbg
	rm -f *.exe
