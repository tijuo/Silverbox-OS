include ../../../prefix.inc

SRC     =time.c mktime.c gmtime.c asctime.c time_decl.c difftime.c\
	ctime.c localtime.c
OBJ     =$(SRC:.c=.o)
OPT_FLG =-O2

all: $(OBJ)

include ../../../Makefile.inc
include ../../../vars.mk

clean:
	rm -f *.o
