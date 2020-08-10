include ../../prefix.inc

SRC     =strtol.c malloc.c algo.c abort.c
OBJ     =$(SRC:.c=.o)
OPT_FLG =-O0 -DLACKS_UNISTD_H -DLACKS_FCNTL_H -DLACKS_SYS_PARAM_H -DLACKS_SYS_MMAN_H \
-DLACKS_STRINGS_H -DLACKS_SCHED_H -DLACKS_TIME_H \
-DHAVE_MMAP=0 -DMALLOC_FAILURE_ACTION=""

all: $(OBJ)

include ../../Makefile.inc
include ../../vars.mk

clean:
	rm -f *.o
