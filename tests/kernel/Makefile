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
		-Wlogical-op -Wpacked -Wno-discarded-qualifiers
DEPDIR		:=.deps
DEPFLAGS	=-MT $@ -MMD -MP -MF $(DEPDIR)/$*.d

all: types buddy_tests
	./buddy_tests

buddy_tests: buddy_tests.c klibc.c $(ROOT_DIR)/kernel/buddy.c $(ROOT_DIR)/munit/munit.c
	$(CC) $(CFLAGS) $^ -o $@

types:
	$(MAKE) -C types all

clean:
	rm -f buddy_tests
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
