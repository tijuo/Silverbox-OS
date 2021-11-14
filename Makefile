include Makefile.inc

.PHONY: all kernel lib apps servers drivers tests clean

QEMU		:=qemu-system-x86_64
QEMU_FLAGS	:=-boot order=acn -fda os_floppy.img -smp cores=2,threads=2 -m 768 -cpu kvm64,kvm
QEMU_DRIVE	:=newimg.img
GDB		:=i686-elf-gdb

all: kernel #libs apps servers drivers tests

kernel:
	$(MAKE) -C kernel all

lib:
	$(MAKE) -C lib all

apps:

servers:

drivers:

kernel/kernel.elf:
	make -C kernel kernel.elf

gdb-debug: kernel/kernel.elf
	$(GDB) kernel/kernel.elf -x gdb.cfg

tests:
	$(MAKE) -C kernel tests
	$(MAKE) -C lib tests

clean:
	$(MAKE) -C kernel clean
	$(MAKE) -C lib clean
