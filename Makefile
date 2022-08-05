.PHONY: all kernel lib apps servers drivers tests clean

QEMU		:=qemu-system-x86_64
QEMU_FLAGS	:=-boot order=acn -fda os_floppy.img -smp cores=2,threads=2 -m 768 -cpu kvm64,kvm
QEMU_DRIVE	:=newimg.img

include Makefile.inc

all: kernel lib servers #apps servers drivers tests

kernel:
	$(MAKE) -C kernel all

lib:
	$(MAKE) -C lib all

apps:

servers:
	$(MAKE) -C servers all

drivers:

kernel/kernel.elf:
	$(MAKE) -C kernel kernel.elf

gdb-debug:
	$(MAKE) -C kernel gdb-debug

tests:
	$(MAKE) -C kernel tests
	$(MAKE) -C lib tests
	$(MAKE) -C servers tests

clean:
	$(MAKE) -C kernel clean
	$(MAKE) -C lib clean
	$(MAKE) -C servers clean
