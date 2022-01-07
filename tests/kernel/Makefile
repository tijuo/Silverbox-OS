.PHONY:	all clean types
.SUFFIXES: .asm .S

export ROOT_DIR		:=../..
export CC		:=gcc
export LD		:=ld
export ASFLAGS		=-I$(ROOT_DIR)/include \
			-Wa,-I$(ROOT_DIR)/include,--64
export DEPEND		:=makedepend
export DD		:=dd
export CFLAGS	=-m64 -I$(ROOT_DIR)/tests/include -std=gnu17 \
		-I$(ROOT_DIR)/munit -O2 -Wall -Wextra -Wshadow \
		-Wpointer-arith -Wsign-compare -Wsystem-headers \
		-Wstrict-prototypes -Wunused-function -Wmissing-prototypes \
		-Wmissing-declarations -Wnonnull \
		-Waddress-of-packed-member -Wno-expansion-to-defined \
		-Werror=nonnull -Winit-self -Wnull-dereference \
		-Wmisleading-indentation -Wswitch-enum -Wswitch-default \
		-Wduplicated-branches -Wduplicated-cond -Wundef \
		-Wcast-align -Wdangling-else -Wno-unused-parameter \
		-Wlogical-op -Wpacked -Wno-discarded-qualifiers -DTEST_MSG \
		-DTEST
LDFLAGS		:=
DEPDIR		:=.deps
DEPFLAGS	=-MT $@ -MMD -MP -MF $(DEPDIR)/$*.d

all: buddy_tests algo_tests mem_tests types
	./buddy_tests
	./mem_tests
	./algo_tests

buddy_tests: buddy_tests.c $(ROOT_DIR)/kernel/buddy.c $(ROOT_DIR)/kernel/types/list.c $(ROOT_DIR)/kernel/types/bitmap.c $(ROOT_DIR)/kernel/sync.c klibc.c $(ROOT_DIR)/munit/munit.c
	$(CC) $(CFLAGS) $^ -o $@

algo_tests: algo_tests.c $(ROOT_DIR)/kernel/algo.c $(ROOT_DIR)/munit/munit.c klibc.c
	$(CC) $(CFLAGS) $^ -o $@

mem_tests: mem_tests.c $(ROOT_DIR)/munit/munit.c $(ROOT_DIR)/kernel/mem.asm
	cp $(ROOT_DIR)/kernel/mem.asm .
	nasm -f elf64 mem.asm -o mem.o
	$(CC) $(CFLAGS) mem_tests.c $(ROOT_DIR)/munit/munit.c mem.o -o $@

types:
	$(MAKE) -C types all

clean:
	rm -f buddy_tests
	rm -f algo_tests
	rm -f mem_tests
	rm -f mem.asm
	rm -f mem.o
	$(MAKE) -C types clean

%.o.c: $(DEPDIR)/%.d | $(DEPDIR)
	$(CC) -c $(DEPFLAGS) $(CFLAGS) $< -o $@

%.o.S: $(DEPDIR)/%.d | $(DEPDIR)
	$(CC) -c $(DEPFLAGS) $(ASFLAGS) $< -o $@

%.o.asm:
	nasm -f elf64 $< -o $@

%.dmp: %
	objdump -Dx $< > $@

%.sym: %
	readelf --syms $< > $@

%.bsym: %
	nm $< | awk -- '{ print $$1, $$3 }' > $@

DEPFILES	:= $(SRC:%.c=$(DEPDIR)/%.d) $(ASM_SRC:%.S=$(DEPDIR)/%.d)

$(DEPFILES):

$(DEPDIR):
	@mkdir -p $@

include $(wildcard $(DEPFILES))
