include ../../../Makefile.inc

.PHONY: all tests clean

ROOT_DIR=../../..
SRC     =memchr.c memcpy.c memset.c strcpy.c strlen.c strcat.c \
	strncpy.c strncat.c memmove.c strcmp.c strncmp.c memcmp.c \
	strchr.c strrchr.c strtok.c strpbrk.c strspn.c strstr.c strcspn.c
OBJ     =$(SRC:.c=.o)

all: $(OBJ)

tests:

clean:
	rm -f *.o

include ../../../Makefile_post.inc
