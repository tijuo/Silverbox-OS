include ../../../Makefile.inc

ROOT_DIR	:=../../..

.PHONY:	all tests clean

SRC	=malloc.c algo.c abort.c strtol.c itoa.c
OBJ	=$(SRC:.c=.o)
CFLAGS	:=$(CFLAGS) -DLACKS_UNISTD_H -DLACKS_FCNTL_H -DLACKS_SYS_PARAM_H -DLACKS_SYS_MMAN_H \
	-DLACKS_STRINGS_H -DLACKS_SCHED_H -DLACKS_TIME_H \
	-DHAVE_MMAP=0 -DMALLOC_FAILURE_ACTION=""

all: $(OBJ)

tests:

clean:
	rm -f *.o

include ../../../Makefile_post.inc
