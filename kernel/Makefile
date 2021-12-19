ROOT_PATH	:=..
include $(ROOT_PATH)/Makefile.inc

.PHONY:	all check clean tests install gdb-debug qemu

QEMU            :=qemu-system-x86_64
QEMU_FLAGS      :=-boot order=cn -smp cores=2,threads=2 -m 768 -cpu kvm64,kvm --enable-kvm -monitor stdio -s -S
QEMU_DRIVE      :=$(ROOT_DIR)/os_image.img

INIT_SRC	:=init/init.c init/acpi.c init/libc.c \
		init/memory.c
TYPES_SRC	:= types/vector.c types/list.c types/bitmap.c

SRC	:=syscall.c debug.c mm.c process.c algo.c\
    	interrupt.c mem.c paging.c schedule.c thread.c \
	apic.c $(INIT_SRC) $(TYPES_SRC)

ASM_SRC	=entry.asm
OBJ	:=$(SRC:%.c=%.o) $(ASM_SRC:%.asm=%.o)
BIN	:=kernel.elf
BIN_GZ	:=kernel.gz
CFLAGS  :=$(CFLAGS) -mpreferred-stack-boundary=4
MNT_PT	:=~/mnt/os_disk

all: $(BIN_GZ) $(BIN).dmp $(BIN).sym $(BIN).bsym

$(BIN): $(OBJ)
	$(LD) $(LDFLAGS) -T link.ld $(OBJ) -o $@

$(BIN_GZ): $(BIN)
	gzip -k $<
	mv $(BIN).gz $@

check:
	splint +gnuextensions -nolib -standard \
	-I$(ROOT_DIR)/include

install: $(BIN_GZ)
	@if [ `losetup -a | grep $(DISK_IMG); echo $?` == "0" ]; then \
	  ~/mount_loop.sh; \
	fi
	@if [ `cat /etc/mtab | grep $(DISK_IMG); echo $?` == "0" ]; then \
	  cp $(BIN_GZ) $(MNT_PT)/boot; \
	else \
	  mount $(MNT_PT); \
	  cp $(BIN_GZ) $(MNT_PT)/boot; \
	  umount $(MNT_PT); \
	fi

gdb-debug: kernel.elf
	$(GDB) kernel.elf -x ../gdb.cfg

qemu: kernel.elf
	$(QEMU) $(QEMU_FLAGS) $(QEMU_DRIVE)

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
