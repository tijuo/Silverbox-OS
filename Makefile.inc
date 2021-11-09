.PHONY: set-release-flags set-debug-flags

export SHELL		:=/bin/bash
export ROOT_DIR		:=$(CURDIR)
export BUILD_DIR	=$(ROOT_DIR)/build
export BIN_DIR		=$(ROOT_DIR)/bin
export TARGET		:=i686-elf
export CC		:=$(TARGET)-gcc
export LD		:=$(TARGET)-ld
export AR		:=$(TARGET)-ar
export ASFLAGS		=-I$(ROOT_DIR)/include \
			-Wa,-I$(ROOT_DIR)/include,--32
export DEPEND		:=makedepend
export DD		:=dd
export EN_DEBUG		:=
export OPT_LEVEL	:=2
export CFLAGS	=-m32 -I$(ROOT_DIR)/include -std=gnu11 \
	     	-fno-leading-underscore -ffreestanding \
		-fno-stack-protector -fno-stack-check \
		-fno-asynchronous-unwind-tables -march=pentium4 \
		$(EN_DEBUG) -O$(OPT_LEVEL) -Wall -Wextra -Wshadow \
		-Wpointer-arith -Wsign-compare -Wsystem-headers \
		-Wstrict-prototypes -Wunused-function -Wmissing-prototypes \
		-Wmissing-declarations -Wnonnull \
		-Waddress-of-packed-member -Wno-expansion-to-defined \
		-Werror=nonnull -Winit-self -Wnull-dereference \
		-Wmisleading-indentation -Wswitch-enum -Wswitch-default \
		-Wduplicated-branches -Wduplicated-cond -Wundef \
		-Wcast-qual -Wcast-align -Wconversion -Wdangling-else \
		-Wlogical-op -Wpacked

export LDFLAGS	:=-static -nostdlib -nodefaultlibs -nostartfiles

set-release-flags:
	eval EN_DEBUG=""
	eval OPT_LEVEL=2

set-debug-flags:
	eval EN_DEBUG="-DDEBUG -g"
	eval OPT_LEVEL=0

.c.o:
	$(CC) -c $(CFLAGS) $< -o $@

.S.o:
	$(CC) -c $(ASFLAGS) $< -o $@
