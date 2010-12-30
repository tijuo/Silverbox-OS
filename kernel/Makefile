#-fno-stack-protector stops the '__stack_chk_fail' problem
# COPTFLAGS are the optimized flags

all:    kernel.gz kernel.dmp kernel.dbg kernel.sym

include ../Makefile.inc
include ../vars.mk

SRC     =init.c paging.c thread.c schedule.c exceptions.c \
        debug.c pic.c queue.c vmm.c message.c syscall.c \
        apic.c signal.c memory.c

OBJ     =$(SRC:.c=.o)
LDFLAGS	=-m elf_i386 -T ../kernelLink.ld --static --exclude-libs ALL

asm_objs:
	make -C asm/ all

kernel.dmp: kernel.tmp
	objdump -Dx $< > $@

kernel.dbg: kernel.tmp
	readelf -a $< > $@

kernel.sym: kernel.dmp
	$(PREFIX)/tools/extract_sym.pl $< > $@

kernel.tmp: $(OBJ) asm_objs
	$(LD) $(LDFLAGS) *.o asm/*.o -o $@

kernel.gz: $(OBJ) asm_objs
	$(LD) $(LDFLAGS) -s -S *.o asm/*.o -o `basename $@ .gz`; \
	rm -f $@
	gzip `basename $@ .gz`

memory.o: memory.c
	$(CC) -O2 -c $(COPTFLAGS) -I$(INC_DIR) $< -o $@

install: kernel.gz
	$(PREFIX)/tools/copy_files.sh kernel.gz

clean:
	make -C asm/ clean
	rm -f *.o
	rm -f kernel.??? kernel.gz

# DO NOT DELETE

init.o: /home/tiju/Silverbox-OS/include/elf.h
init.o: /home/tiju/Silverbox-OS/include/kernel/thread.h
init.o: /home/tiju/Silverbox-OS/include/types.h
init.o: /home/tiju/Silverbox-OS/include/stddef.h
init.o: /home/tiju/Silverbox-OS/include/stdbool.h
init.o: /home/tiju/Silverbox-OS/include/oslib.h
init.o: /home/tiju/Silverbox-OS/include/ipc.h
init.o: /home/tiju/Silverbox-OS/include/os/mutex.h
init.o: /home/tiju/Silverbox-OS/include/kernel/mm.h
init.o: /home/tiju/Silverbox-OS/include/kernel/multiboot.h
init.o: /home/tiju/Silverbox-OS/include/kernel/lowlevel.h
init.o: /home/tiju/Silverbox-OS/include/kernel/pit.h
init.o: /home/tiju/Silverbox-OS/include/kernel/paging.h
init.o: /home/tiju/Silverbox-OS/include/kernel/debug.h
init.o: /home/tiju/Silverbox-OS/include/kernel/schedule.h
init.o: /home/tiju/Silverbox-OS/include/os/io.h
init.o: /home/tiju/Silverbox-OS/include/kernel/queue.h
init.o: /home/tiju/Silverbox-OS/include/kernel/cpuid.h
init.o: /home/tiju/Silverbox-OS/include/kernel/memory.h
init.o: /home/tiju/Silverbox-OS/include/kernel/rtc.h
init.o: /home/tiju/Silverbox-OS/include/os/variables.h
paging.o: /home/tiju/Silverbox-OS/include/kernel/mm.h
paging.o: /home/tiju/Silverbox-OS/include/types.h
paging.o: /home/tiju/Silverbox-OS/include/stddef.h
paging.o: /home/tiju/Silverbox-OS/include/stdbool.h
paging.o: /home/tiju/Silverbox-OS/include/kernel/multiboot.h
paging.o: /home/tiju/Silverbox-OS/include/kernel/lowlevel.h
paging.o: /home/tiju/Silverbox-OS/include/kernel/pit.h
paging.o: /home/tiju/Silverbox-OS/include/kernel/paging.h
paging.o: /home/tiju/Silverbox-OS/include/oslib.h
paging.o: /home/tiju/Silverbox-OS/include/ipc.h
paging.o: /home/tiju/Silverbox-OS/include/os/mutex.h
paging.o: /home/tiju/Silverbox-OS/include/kernel/debug.h
paging.o: /home/tiju/Silverbox-OS/include/kernel/thread.h
paging.o: /home/tiju/Silverbox-OS/include/kernel/memory.h
thread.o: /home/tiju/Silverbox-OS/include/kernel/debug.h
thread.o: /home/tiju/Silverbox-OS/include/kernel/mm.h
thread.o: /home/tiju/Silverbox-OS/include/types.h
thread.o: /home/tiju/Silverbox-OS/include/stddef.h
thread.o: /home/tiju/Silverbox-OS/include/stdbool.h
thread.o: /home/tiju/Silverbox-OS/include/kernel/multiboot.h
thread.o: /home/tiju/Silverbox-OS/include/kernel/lowlevel.h
thread.o: /home/tiju/Silverbox-OS/include/kernel/pit.h
thread.o: /home/tiju/Silverbox-OS/include/kernel/paging.h
thread.o: /home/tiju/Silverbox-OS/include/oslib.h
thread.o: /home/tiju/Silverbox-OS/include/ipc.h
thread.o: /home/tiju/Silverbox-OS/include/os/mutex.h
thread.o: /home/tiju/Silverbox-OS/include/string.h
thread.o: /home/tiju/Silverbox-OS/include/kernel/thread.h
thread.o: /home/tiju/Silverbox-OS/include/kernel/schedule.h
thread.o: /home/tiju/Silverbox-OS/include/os/io.h
thread.o: /home/tiju/Silverbox-OS/include/kernel/queue.h
schedule.o: /home/tiju/Silverbox-OS/include/kernel/thread.h
schedule.o: /home/tiju/Silverbox-OS/include/types.h
schedule.o: /home/tiju/Silverbox-OS/include/stddef.h
schedule.o: /home/tiju/Silverbox-OS/include/stdbool.h
schedule.o: /home/tiju/Silverbox-OS/include/oslib.h
schedule.o: /home/tiju/Silverbox-OS/include/ipc.h
schedule.o: /home/tiju/Silverbox-OS/include/os/mutex.h
schedule.o: /home/tiju/Silverbox-OS/include/kernel/mm.h
schedule.o: /home/tiju/Silverbox-OS/include/kernel/multiboot.h
schedule.o: /home/tiju/Silverbox-OS/include/kernel/lowlevel.h
schedule.o: /home/tiju/Silverbox-OS/include/kernel/pit.h
schedule.o: /home/tiju/Silverbox-OS/include/kernel/paging.h
schedule.o: /home/tiju/Silverbox-OS/include/kernel/debug.h
schedule.o: /home/tiju/Silverbox-OS/include/kernel/memory.h
schedule.o: /home/tiju/Silverbox-OS/include/kernel/schedule.h
schedule.o: /home/tiju/Silverbox-OS/include/os/io.h
schedule.o: /home/tiju/Silverbox-OS/include/kernel/queue.h
exceptions.o: /home/tiju/Silverbox-OS/include/kernel/thread.h
exceptions.o: /home/tiju/Silverbox-OS/include/types.h
exceptions.o: /home/tiju/Silverbox-OS/include/stddef.h
exceptions.o: /home/tiju/Silverbox-OS/include/stdbool.h
exceptions.o: /home/tiju/Silverbox-OS/include/oslib.h
exceptions.o: /home/tiju/Silverbox-OS/include/ipc.h
exceptions.o: /home/tiju/Silverbox-OS/include/os/mutex.h
exceptions.o: /home/tiju/Silverbox-OS/include/kernel/mm.h
exceptions.o: /home/tiju/Silverbox-OS/include/kernel/multiboot.h
exceptions.o: /home/tiju/Silverbox-OS/include/kernel/lowlevel.h
exceptions.o: /home/tiju/Silverbox-OS/include/kernel/pit.h
exceptions.o: /home/tiju/Silverbox-OS/include/kernel/paging.h
exceptions.o: /home/tiju/Silverbox-OS/include/kernel/debug.h
exceptions.o: /home/tiju/Silverbox-OS/include/kernel/memory.h
exceptions.o: /home/tiju/Silverbox-OS/include/os/signal.h
debug.o: /home/tiju/Silverbox-OS/include/kernel/debug.h
debug.o: /home/tiju/Silverbox-OS/include/kernel/mm.h
debug.o: /home/tiju/Silverbox-OS/include/types.h
debug.o: /home/tiju/Silverbox-OS/include/stddef.h
debug.o: /home/tiju/Silverbox-OS/include/stdbool.h
debug.o: /home/tiju/Silverbox-OS/include/kernel/multiboot.h
debug.o: /home/tiju/Silverbox-OS/include/kernel/lowlevel.h
debug.o: /home/tiju/Silverbox-OS/include/kernel/pit.h
debug.o: /home/tiju/Silverbox-OS/include/kernel/paging.h
debug.o: /home/tiju/Silverbox-OS/include/oslib.h
debug.o: /home/tiju/Silverbox-OS/include/ipc.h
debug.o: /home/tiju/Silverbox-OS/include/os/mutex.h
debug.o: /home/tiju/Silverbox-OS/include/kernel/memory.h
debug.o: /home/tiju/Silverbox-OS/include/kernel/thread.h
debug.o: /home/tiju/Silverbox-OS/include/os/io.h
debug.o: /home/tiju/Silverbox-OS/include/stdarg.h
pic.o: /home/tiju/Silverbox-OS/include/os/io.h
pic.o: /home/tiju/Silverbox-OS/include/types.h
pic.o: /home/tiju/Silverbox-OS/include/stddef.h
pic.o: /home/tiju/Silverbox-OS/include/stdbool.h
pic.o: /home/tiju/Silverbox-OS/include/kernel/pic.h
queue.o: /home/tiju/Silverbox-OS/include/kernel/queue.h
queue.o: /home/tiju/Silverbox-OS/include/types.h
queue.o: /home/tiju/Silverbox-OS/include/stddef.h
queue.o: /home/tiju/Silverbox-OS/include/stdbool.h
queue.o: /home/tiju/Silverbox-OS/include/kernel/thread.h
queue.o: /home/tiju/Silverbox-OS/include/oslib.h
queue.o: /home/tiju/Silverbox-OS/include/ipc.h
queue.o: /home/tiju/Silverbox-OS/include/os/mutex.h
queue.o: /home/tiju/Silverbox-OS/include/kernel/mm.h
queue.o: /home/tiju/Silverbox-OS/include/kernel/multiboot.h
queue.o: /home/tiju/Silverbox-OS/include/kernel/lowlevel.h
queue.o: /home/tiju/Silverbox-OS/include/kernel/pit.h
queue.o: /home/tiju/Silverbox-OS/include/kernel/paging.h
queue.o: /home/tiju/Silverbox-OS/include/kernel/debug.h
vmm.o: /home/tiju/Silverbox-OS/include/kernel/mm.h
vmm.o: /home/tiju/Silverbox-OS/include/types.h
vmm.o: /home/tiju/Silverbox-OS/include/stddef.h
vmm.o: /home/tiju/Silverbox-OS/include/stdbool.h
vmm.o: /home/tiju/Silverbox-OS/include/kernel/multiboot.h
vmm.o: /home/tiju/Silverbox-OS/include/kernel/lowlevel.h
vmm.o: /home/tiju/Silverbox-OS/include/kernel/pit.h
vmm.o: /home/tiju/Silverbox-OS/include/kernel/paging.h
vmm.o: /home/tiju/Silverbox-OS/include/kernel/syscall.h
vmm.o: /home/tiju/Silverbox-OS/include/kernel/thread.h
vmm.o: /home/tiju/Silverbox-OS/include/oslib.h
vmm.o: /home/tiju/Silverbox-OS/include/ipc.h
vmm.o: /home/tiju/Silverbox-OS/include/os/mutex.h
vmm.o: /home/tiju/Silverbox-OS/include/kernel/memory.h
vmm.o: /home/tiju/Silverbox-OS/include/kernel/debug.h
message.o: /home/tiju/Silverbox-OS/include/kernel/mm.h
message.o: /home/tiju/Silverbox-OS/include/types.h
message.o: /home/tiju/Silverbox-OS/include/stddef.h
message.o: /home/tiju/Silverbox-OS/include/stdbool.h
message.o: /home/tiju/Silverbox-OS/include/kernel/multiboot.h
message.o: /home/tiju/Silverbox-OS/include/kernel/lowlevel.h
message.o: /home/tiju/Silverbox-OS/include/kernel/pit.h
message.o: /home/tiju/Silverbox-OS/include/kernel/paging.h
message.o: /home/tiju/Silverbox-OS/include/kernel/debug.h
message.o: /home/tiju/Silverbox-OS/include/oslib.h
message.o: /home/tiju/Silverbox-OS/include/ipc.h
message.o: /home/tiju/Silverbox-OS/include/os/mutex.h
message.o: /home/tiju/Silverbox-OS/include/kernel/thread.h
message.o: /home/tiju/Silverbox-OS/include/kernel/schedule.h
message.o: /home/tiju/Silverbox-OS/include/os/io.h
message.o: /home/tiju/Silverbox-OS/include/kernel/queue.h
message.o: /home/tiju/Silverbox-OS/include/kernel/syscall.h
message.o: /home/tiju/Silverbox-OS/include/kernel/memory.h
message.o: /home/tiju/Silverbox-OS/include/os/message.h
message.o: /home/tiju/Silverbox-OS/include/os/signal.h
syscall.o: /home/tiju/Silverbox-OS/include/kernel/memory.h
syscall.o: /home/tiju/Silverbox-OS/include/types.h
syscall.o: /home/tiju/Silverbox-OS/include/stddef.h
syscall.o: /home/tiju/Silverbox-OS/include/stdbool.h
syscall.o: /home/tiju/Silverbox-OS/include/kernel/mm.h
syscall.o: /home/tiju/Silverbox-OS/include/kernel/multiboot.h
syscall.o: /home/tiju/Silverbox-OS/include/kernel/lowlevel.h
syscall.o: /home/tiju/Silverbox-OS/include/kernel/pit.h
syscall.o: /home/tiju/Silverbox-OS/include/kernel/paging.h
syscall.o: /home/tiju/Silverbox-OS/include/kernel/debug.h
syscall.o: /home/tiju/Silverbox-OS/include/oslib.h
syscall.o: /home/tiju/Silverbox-OS/include/ipc.h
syscall.o: /home/tiju/Silverbox-OS/include/os/mutex.h
syscall.o: /home/tiju/Silverbox-OS/include/kernel/syscall.h
syscall.o: /home/tiju/Silverbox-OS/include/kernel/thread.h
syscall.o: /home/tiju/Silverbox-OS/include/kernel/schedule.h
syscall.o: /home/tiju/Silverbox-OS/include/os/io.h
syscall.o: /home/tiju/Silverbox-OS/include/kernel/queue.h
syscall.o: /home/tiju/Silverbox-OS/include/kernel/error.h
syscall.o: /home/tiju/Silverbox-OS/include/kernel/message.h
syscall.o: /home/tiju/Silverbox-OS/include/kernel/signal.h
syscall.o: /home/tiju/Silverbox-OS/include/os/signal.h
apic.o: /home/tiju/Silverbox-OS/include/kernel/apic.h
apic.o: /home/tiju/Silverbox-OS/include/kernel/mm.h
apic.o: /home/tiju/Silverbox-OS/include/types.h
apic.o: /home/tiju/Silverbox-OS/include/stddef.h
apic.o: /home/tiju/Silverbox-OS/include/stdbool.h
apic.o: /home/tiju/Silverbox-OS/include/kernel/multiboot.h
apic.o: /home/tiju/Silverbox-OS/include/kernel/lowlevel.h
apic.o: /home/tiju/Silverbox-OS/include/kernel/pit.h
apic.o: /home/tiju/Silverbox-OS/include/kernel/paging.h
apic.o: /home/tiju/Silverbox-OS/include/kernel/debug.h
apic.o: /home/tiju/Silverbox-OS/include/oslib.h
apic.o: /home/tiju/Silverbox-OS/include/ipc.h
apic.o: /home/tiju/Silverbox-OS/include/os/mutex.h
signal.o: /home/tiju/Silverbox-OS/include/kernel/signal.h
signal.o: /home/tiju/Silverbox-OS/include/os/signal.h
signal.o: /home/tiju/Silverbox-OS/include/kernel/thread.h
signal.o: /home/tiju/Silverbox-OS/include/types.h
signal.o: /home/tiju/Silverbox-OS/include/stddef.h
signal.o: /home/tiju/Silverbox-OS/include/stdbool.h
signal.o: /home/tiju/Silverbox-OS/include/oslib.h
signal.o: /home/tiju/Silverbox-OS/include/ipc.h
signal.o: /home/tiju/Silverbox-OS/include/os/mutex.h
signal.o: /home/tiju/Silverbox-OS/include/kernel/mm.h
signal.o: /home/tiju/Silverbox-OS/include/kernel/multiboot.h
signal.o: /home/tiju/Silverbox-OS/include/kernel/lowlevel.h
signal.o: /home/tiju/Silverbox-OS/include/kernel/pit.h
signal.o: /home/tiju/Silverbox-OS/include/kernel/paging.h
signal.o: /home/tiju/Silverbox-OS/include/string.h
signal.o: /home/tiju/Silverbox-OS/include/kernel/queue.h
signal.o: /home/tiju/Silverbox-OS/include/kernel/debug.h
memory.o: /home/tiju/Silverbox-OS/include/kernel/memory.h
memory.o: /home/tiju/Silverbox-OS/include/types.h
memory.o: /home/tiju/Silverbox-OS/include/stddef.h
memory.o: /home/tiju/Silverbox-OS/include/stdbool.h
