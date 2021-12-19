.PHONY: all kernel install lib apps drivers tests clean qemu gdb-debug bochs

QEMU		:=qemu-system-x86_64
QEMU_FLAGS	:=-boot order=cn -smp cores=2,threads=2 -m 768 -cpu kvm64,kvm
QEMU_DRIVE	:=os_floppy.img

include Makefile.inc

all: kernel #lib apps servers drivers tests

kernel:
	$(MAKE) -C kernel all

lib:
	$(MAKE) -C lib all

apps:

servers:
	$(MAKE) -C servers all

install:
	$(MAKE) -C kernel install

drivers:

kernel/kernel.elf:
	$(MAKE) -C kernel kernel.elf

gdb-debug:
	$(MAKE) -C kernel gdb-debug

qemu:
	$(MAKE) -C kernel qemu

bochs:
	rm -f os_image.img.lock
	bochs -f bochsrc.bxrc

tests:
	$(MAKE) -C kernel tests
	$(MAKE) -C lib tests
	$(MAKE) -C tests all

clean:
	$(MAKE) -C kernel clean
	$(MAKE) -C lib clean
	$(MAKE) -C tests clean
