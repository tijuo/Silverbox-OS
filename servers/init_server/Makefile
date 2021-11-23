include ../../Makefile.inc

ROOT_DIR	:=../..

INSTALL_DIR=sbos/servers/
OUTPUT=target/i686-unknown-elf/debug/init_server.exe
EXE_NAME=init.exe
DMP_NAME=init.dmp

all: $(EXE_NAME) $(DMP_NAME)

$(DMP_NAME): $(EXE_NAME)
	objdump -g -Dx -M intel $< > $@

$(EXE_NAME): $(OUTPUT)
	cp $(OUTPUT) $(EXE_NAME)

$(OUTPUT): $(ROOT_DIR)/lib/libc/libc.a $(ROOT_DIR)/lib/libos/libos_init.a \
  $(ROOT_DIR)/lib/librust/librust.rlib src/*.rs
	cargo build
#	strip $(OUTPUT)

install: $(EXE_NAME)
	$(ROOT_DIR)/tools/copy_files.sh -d $(INSTALL_DIR) $(EXE_NAME)

clean:
	cargo clean
	rm -f *.dmp
	rm -f $(EXE_NAME)

test:
	cargo test

include ../../Makefile_post.inc
