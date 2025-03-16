ROOT_PATH	:=..
include $(ROOT_PATH)/Makefile.inc

.PHONY:	all check clean tests install

SRC	:=list.c message.c syscall.c debug.c \
    	interrupt.c mem.c paging.c schedule.c thread.c \
	apic.c init/init.c init/acpi.c init/loader.c init/libc.c \
	init/memory.c
ASM_SRC	=entry.S
OBJ	:=$(SRC:%.c=%.o) $(ASM_SRC:%.S=%.o)
BIN	:=kernel.elf
BIN_GZ	:=kernel.gz
CFLAGS  :=$(CFLAGS) -mpreferred-stack-boundary=4

#CHECK_INC=/opt/cross/i686-elf/lib/gcc/i686-elf/12.0.0

all: $(BIN_GZ) $(BIN).dmp $(BIN).sym $(BIN).bsym

$(BIN): $(OBJ) link.ld
	$(LD) $(LDFLAGS) -T link.ld $(OBJ) -o $@

$(BIN_GZ): $(BIN)
	gzip -k $<
	mv $(BIN).gz $@

check:
	splint +gnuextensions -nolib -standard \
	-I$(ROOT_DIR)/include #-I$(CHECK_INC)/include \
#	-I$(CHECK_INC)/include-fixed *.c

install: $(BIN_GZ)
#	$(ROOT_DIR)/tools/copy_files.sh -d boot $<
	~/mount_loop.sh
	mount ~/os_disk
	cp $(BIN_GZ) ~/os_disk/boot || umount ~/os_disk
	umount ~/os_disk
	losetup -d /dev/loop6
	rm -f $(DISK_IMG).lock

tests:

clean:
	rm -f $(OBJ)
	rm -f $(BIN)
	rm -f $(BIN).dmp
	rm -f $(BIN).gz
	rm -f $(BIN_GZ)
	rm -f $(BIN).sym
	rm -f $(BIN).bsym

include ../Makefile_post.inc
