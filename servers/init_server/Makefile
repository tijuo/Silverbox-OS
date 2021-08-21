include ../../prefix.inc

INSTALL_DIR=sbos/servers/
OUTPUT=target/i686-unknown-elf/debug/init_server.exe
EXE_NAME=init.exe

all: $(SB_PREFIX)/lib/libc/libc.a $(SB_PREFIX)/lib/libos/libos.a
	cargo build
	@objdump -g -Dx -M intel $(OUTPUT) > `basename $(OUTPUT) .exe`.dmp
	strip $(OUTPUT)

$(OUTPUT): $(SB_PREFIX)/lib/libc/libc.a $(SB_PREFIX)/lib/libos/libos.a
	cargo build
	@objdump -g -Dx -M intel $(OUTPUT) > `basename $(OUTPUT) .exe`.dmp
	strip $(OUTPUT)

install: $(OUTPUT)
	cp $(OUTPUT) $(EXE_NAME)
	$(SB_PREFIX)/tools/copy_files.sh $(EXE_NAME) -d $(INSTALL_DIR)

clean:
	cargo clean
	rm -f *.dmp
	rm -f $(EXE_NAME)

test:
	cargo test

include ../../vars.mk
include ../../Makefile.inc
