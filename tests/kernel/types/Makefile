.PHONY:	all clean
.SUFFIXES: .asm .S

ROOT_DIR	:=../../..
CC		:=gcc
LD		:=ld
ASFLAGS		:=-I$(ROOT_DIR)/include -Wa,-I$(ROOT_DIR)/include,--64
DEPEND		:=makedepend
DD		:=dd
CFLAGS		:=-m64 -std=c17 -pedantic -O0 -g -Wall -Wextra \
		-I$(ROOT_DIR)/munit -I../../include -Wshadow \
		-Wpointer-arith -Wsign-compare -Wsystem-headers \
		-Wstrict-prototypes -Wno-unused-parameter -Wmissing-prototypes \
		-Wmissing-declarations -Wnonnull \
		-Waddress-of-packed-member -Wno-expansion-to-defined \
		-Werror=nonnull -Winit-self -Wnull-dereference \
		-Wmisleading-indentation -Wswitch-enum -Wswitch-default \
		-Wduplicated-branches -Wduplicated-cond -Wundef \
		-Wcast-align -Wdangling-else -Wno-unused-parameter \
		-Wno-unused-function \
		-Wlogical-op -Wpacked -Wno-discarded-qualifiers
DEPDIR		:=.deps
DEPFLAGS	=-MT $@ -MMD -MP -MF $(DEPDIR)/$*.d
SRC		:=kmalloc.c $(ROOT_DIR)/munit/munit.c


all: vector_tests bitmap_tests
	./vector_tests
	./bitmap_tests

vector_tests: vector_tests.c $(ROOT_DIR)/kernel/types/vector.c $(SRC)
	$(CC) $(CFLAGS) $^ -o $@

bitmap_tests: bitmap_tests.c $(ROOT_DIR)/kernel/types/bitmap.c $(SRC)
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -f vector_tests
	rm -f bitmap_tests

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

$(OBJ_DIR):
	@mkdir -p $@

$(BIN_DIR):
	@mkdir -p $@

include $(wildcard $(DEPFILES))
