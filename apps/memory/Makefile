CEXTRAFL=-Wpacked -Wpadded
CXXFLAGS=-O0 -fno-rtti --fno-exceptions -nostdlib -nostartfiles -nodefaultlibs --fno-builtin -nostartfiles
INC_DIR =../include/ 
APPS_DIR=../
LFLAGS  =-melf_i386 -T ../../link.ld
ASFLAGS =-f elf
CC      =gcc
LD      =ld
AS      =nasm
AR      =ar
TMP	=../tmp

OUTPUT  =pagersrv.exe

SRC	=pagersrv.cpp AllocTable.cpp

all: $(SRC)
	$(SILVERBOX)/tools/makecpp.sh $(SRC) $(OUTPUT)
	objdump -Dx --demangle $(OUTPUT) > `basename $(OUTPUT) .exe`.dmp
update:
	mount /mnt/osfloppy
	cp pagersrv.exe /mnt/osfloppy
	umount /mnt/osfloppy

clean:
	rm -f *.o
	rm -f *.dbg
	rm -f *.dmp
	rm -f $(OUTPUT)
