include ../Makefile.inc

.PHONY: all tests init_server clean init_server-tests init_server-clean

all: init_server

init_server:
	make -C init_server all

init_server-tests:
	make -C init_server tests

tests: init_server-tests

clean: init_server-clean

init_server-clean:
	make -C init_server clean
