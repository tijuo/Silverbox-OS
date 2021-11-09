include Makefile.inc

.PHONY: all kernel libs apps servers drivers tests clean

all: kernel libs apps servers drivers tests

kernel:
	$(MAKE) -C kernel all

libs:
	$(MAKE) -C libs all

apps:

servers:

drivers:

tests:
	$(MAKE) -C kernel tests
	$(MAKE) -C libs tests

clean:
	$(MAKE) -C kernel clean
	$(MAKE) -C libs all
