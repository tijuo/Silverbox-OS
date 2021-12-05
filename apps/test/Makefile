include ../../Makefile.inc

ROOT_DIR	:=../..

INSTALL_DIR	=programs/
OBJ     	:=load_test.o $(ROOT_DIR)/tools/crt0.o
OUTPUT		=load_test.elf
DMP_NAME	=load_test.dmp
CFLAGS		:=-mcmodel=large $(CFLAGS)

.PHONY: all clean gdb-debug tests

all: $(OUTPUT) $(DMP_NAME)

$(DMP_NAME): $(OUTPUT)
	objdump -C -g -Dx -M intel $< > $@

$(OUTPUT): $(ROOT_DIR)/lib/libc/libc.a $(ROOT_DIR)/lib/libos/libos_init.a \
  $(OBJ)
	$(LD) -L $(ROOT_DIR)/lib -L /opt/cross/lib/gcc/x86_64-elf/12.0.0/ \
  -T ../link.ld $(LDFLAGS) $(OBJ) -lgcc -lc -los -o $@

install: $(OUTPUT)
	~/mount_loop.sh
	mount ~/os_disk
	cp $(OUTPUT) ~/os_disk/$(INSTALL_DIR) || umount ~/os_disk
	umount ~/os_disk
	losetup -d /dev/loop6
	rm -f $(DISK_IMG).lock

clean:
	rm -f load_test.o
	rm -f *.dmp
	rm -f $(OUTPUT)

tests:

gdb-debug: $(OUTPUT)
	$(GDB) $(OUTPUT) -x $(ROOT_DIR)/gdb.cfg

include ../../Makefile_post.inc
