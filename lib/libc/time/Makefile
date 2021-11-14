include ../../../Makefile.inc

ROOT_DIR=../../..
.PHONY:	all tests clean

SRC     =time.c mktime.c gmtime.c asctime.c time_decl.c difftime.c\
	ctime.c localtime.c
OBJ     =$(SRC:.c=.o)

all: $(OBJ)

tests:

clean:
	rm -f *.o

include ../../../Makefile_post.inc
