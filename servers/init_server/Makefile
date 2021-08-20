include ../../prefix.inc

INSTALL_DIR=sbos/servers/
OUTPUT=target/i686-unknown-elf/debug/init_server.exe

all: $(OUTPUT)
	@objdump -g -Dx -M intel $(OUTPUT) > `basename $(OUTPUT) .exe`.dmp
	make install

$(OUTPUT): $(SB_PREFIX)/lib/libc/libc.a $(SB_PREFIX)/lib/libos/libos.a
	cargo build
	strip $(OUTPUT)

install: $(OUTPUT)
	@objdump -g -Dx -M intel $(OUTPUT) > `basename $(OUTPUT) .exe`.dmp
	$(SB_PREFIX)/tools/copy_files.sh $(OUTPUT) -d $(INSTALL_DIR)

clean:
	cargo clean
	rm -f *.dmp

test:
	cargo test

include ../../vars.mk
include ../../Makefile.inc
