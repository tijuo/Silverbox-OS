.PHONY:	all clean

export RELEASE		=
export COMPILER_INCL    =/opt/cross-compiler/lib/gcc/i686-elf/15.0.0
export SHELL		:=/bin/bash
export ROOT_DIR		=/home/user/programming/C/Silverbox-OS#$(CURDIR)/$(ROOT_PATH)
export BUILD_DIR	=$(ROOT_DIR)/build
export BIN_DIR		=$(ROOT_DIR)/bin
export TARGET		:=i686-elf
export AS               :=$(TARGET)-as
export CC		    :=$(TARGET)-gcc
export LD		    :=$(TARGET)-ld
export AR		    :=$(TARGET)-ar
export ASFLAGS		=-I$(ROOT_DIR)/include
export DEPEND		:=makedepend
export DD		    :=dd
export EN_DEBUG		:=$(if $(RELEASE),,-DDEBUG -g)
export OPT_LEVEL	:=$(if $(RELEASE),2,0)
export CFLAGS	    =-march=pentium4 -msse -msse2 -I$(ROOT_DIR)/lib/pdclib/include -I$(ROOT_DIR)/include -I$(COMPILER_INCL)/include -std=gnu17 \
	            -fno-leading-underscore -ffreestanding -nostartfiles \
		-fno-stack-protector -fno-stack-check \
		-fno-asynchronous-unwind-tables \
		$(if $(RELEASE),,-DDEBUG -ggdb -g) \
		-O$(OPT_LEVEL) -Wall -Wextra -Wshadow \
		-Wpointer-arith -Wsign-compare -Wsystem-headers \
		-Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations \
		-Waddress-of-packed-member -Wno-expansion-to-defined \
		-Werror=nonnull -Winit-self -Wnull-dereference \
		-Wmisleading-indentation -Wswitch-enum -Wswitch-default \
		-Wduplicated-branches -Wduplicated-cond -Wno-undef \
		-Wcast-align -Wdangling-else \
		-Wlogical-op -Wpacked -Wno-discarded-qualifiers -Wbad-function-cast \
        -Wnested-externs -fanalyzer -Wno-analyzer-va-arg-type-mismatch
export LDFLAGS	:=$(if $(RELEASE),,-g) -static -nostdlib -z nodefaultlib -L$(COMPILER_INCL)
export DISK_IMG =$(ROOT_DIR)/os_image.img
export GDB	    :=$(TARGET)-gdb
TOP		        =$(shell if [ -f "Makefile.top" ]; then echo 1; else echo ""; fi)
DEPDIR		    :=.deps
DEPFLAGS	    =-MT $@ -MMD -MP -MF $(DEPDIR)/$*.d
TESTDIR		    :=test

%.o: %.c $(DEPDIR)/%.d | $(DEPDIR)
	$(CC) -c $(DEPFLAGS) $(CFLAGS) $< -o $@

%.o: %.S $(DEPDIR)/%.d | $(DEPDIR)
	$(AS) -c $(DEPFLAGS) $(ASFLAGS) $< -o $@

%.dmp: %
	objdump -Dx $< > $@

%.sym: %
	readelf --syms $< > $@

%.globals.dmp: %
	objdump -x -j ".data" -j ".bss" $< > $@

%.bsym: %
	nm $< | awk -- '{ print $$1, $$3 }' > $@

DEPFILES	:= $(SRC:%.c=$(DEPDIR)/%.d) $(ASM_SRC:%.S=$(DEPDIR)/%.d)
