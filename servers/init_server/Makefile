include ../../Makefile.inc

ROOT_DIR	:=../..

INSTALL_DIR=sbos/servers/
OUTPUT=target/i686-pc-none-elf/debug/init_server.exe
EXE_NAME=init.exe
DMP_NAME=init.dmp

.PHONY: all clean gdb-debug tests

all: $(EXE_NAME) $(DMP_NAME)

$(DMP_NAME): $(EXE_NAME)
	objdump -C -Dx -M intel $< > $@

$(EXE_NAME): $(OUTPUT)
	cp $(OUTPUT) $(EXE_NAME)

$(OUTPUT): $(ROOT_DIR)/lib/libc.a $(ROOT_DIR)/lib/libos/libos_init.a \
  $(ROOT_DIR)/lib/librust/librust.rlib src/*.rs
	cargo build
#	strip $(OUTPUT)

install: $(EXE_NAME)
	cp $(EXE_NAME) ~/tftp/boot

clean:
	cargo clean
	rm -f *.dmp
	rm -f $(EXE_NAME)

tests:
	cargo test

gdb-debug: $(OUTPUT)
	$(GDB) $(OUTPUT) -x $(ROOT_DIR)/gdb.cfg

include ../../Makefile_post.inc
