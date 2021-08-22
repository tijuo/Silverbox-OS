include ../../../prefix.inc

SRC     =cqueue.c sbarray.c sbhash.c sbstring.c circbuffer.c
OBJ     =$(SRC:.c=.o)
OPT_FLG =-O0

all: $(OBJ)

include ../../../Makefile.inc
include ../../../vars.mk

clean:
	rm -f *.o
