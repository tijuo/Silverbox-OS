include ../prefix.inc

#-fno-stack-protector stops the '__stack_chk_fail' problem
# COPTFLAGS are the optimized flags

all:    kernel.gz kernel.dmp kernel.dbg kernel.sym

include ../Makefile.inc
include ../vars.mk

SRC     =init.c paging.c thread.c schedule.c interrupt.c \
        debug.c pic.c message.c syscall.c mem.c \
	list.c

OBJ     =$(SRC:.c=.o)
LDFLAGS	=-m elf_i386 -T ../kernelLink.ld --static --exclude-libs ALL
OPT_FLG =-O0

asm_objs:
	make -C asm/ all

kernel.dmp: kernel.tmp
	objdump -Dx -g $< > $@

kernel.dbg: kernel.tmp
	readelf -a $< > $@

kernel.sym: kernel.dmp
	$(SB_PREFIX)/tools/extract_sym.pl $< > $@

kernel.tmp: $(OBJ) asm_objs
	$(LD) $(LDFLAGS) *.o asm/*.o -o $@

kernel.gz: $(OBJ) asm_objs
	$(LD) $(LDFLAGS) -s -S *.o asm/*.o -o kernel && \
	gzip -k -f kernel

#memory.o: memory.c
#	$(CC) -O2 -c $(CFLAGS) -I$(INC_DIR) $< -o $@

install: kernel.gz
	$(SB_PREFIX)/tools/copy_files.sh -d /boot kernel.gz

check:
	splint +gnuextensions -nolib -standard  -I../include *.c

clean:
	make -C asm/ clean
	rm -f *.o
	rm -f kernel.dmp
	rm -f kernel.tmp
	rm -f kernel.sym
	rm -f kernel.dbg
	rm -f kernel
	rm -f kernel.gz
