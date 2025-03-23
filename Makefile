.PHONY: all kernel lib apps servers drivers tests clean

TFTP_ROOT       =$$HOME/tftp
PXE_IMG         =/boot/limine/limine-bios-pxe.bin

QEMU		:=qemu-system-i386
QEMU_FLAGS	:=-netdev user,id="net0",net=192.168.2.0/24,tftp="$(TFTP_ROOT)",bootfile="$(PXE_IMG)" -device virtio-net-pci,netdev=net0 -object rng-random,id=virtio-rng0,filename=/dev/urandom -device virtio-rng-pci,rng=virtio-rng0,id=rng0,bus=pci.0,addr=0x9 -m 2048 -debugcon stdio -s -no-shutdown -no-reboot -D qemu.log -boot order=nca 
QEMU_DRIVE	:=newimg.img

include Makefile.inc

all: kernel libs servers #apps servers drivers tests

kernel:
	$(MAKE) -C kernel all

lib:
	$(MAKE) -C lib all

apps:

servers:
	$(MAKE) -C servers all

drivers:

kernel/kernel.elf:
	make -C kernel kernel.elf

gdb-debug: kernel/kernel.elf
	$(GDB) kernel/kernel.elf -x gdb.cfg

qemu: kernel/kernel.gz
	$(QEMU) $(QEMU_FLAGS)

qemu-debug: kernel/kernel.gz
	$(QEMU) $(QEMU_FLAGS) -S

tests:
	$(MAKE) -C kernel tests
	$(MAKE) -C lib tests
	$(MAKE) -C servers tests

clean:
	$(MAKE) -C kernel clean
	$(MAKE) -C lib clean
	$(MAKE) -C servers clean
