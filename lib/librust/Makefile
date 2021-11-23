include ../../Makefile.inc

ROOT_DIR=../..
LIB_NAME=librust.rlib
LIB	=target/i686-unknown-elf/debug/$(LIB_NAME)

.PHONY: all tests clean

all: $(LIB_NAME)

$(LIB): src/*.rs
	cargo build

$(LIB_NAME): $(LIB)
	cp $(LIB) $(LIB_NAME)

tests:
	cargo test

clean:
	cargo clean
	rm -f librust.rlib
