.PHONY:	all clean tests
.SUFFIXES: .asm .S

export ROOT_DIR		=..

all: tests

tests:
	$(MAKE) -C kernel all

clean:
	$(MAKE) -C kernel clean
