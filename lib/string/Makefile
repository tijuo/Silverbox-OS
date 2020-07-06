SRC     =memchr.c memcpy.c memset.c strcpy.c strlen.c strcat.c \
	strncpy.c strncat.c memmove.c strcmp.c strncmp.c memcmp.c \
	strchr.c strrchr.c strtok.c strpbrk.c strspn.c strstr.c strcspn.c
OBJ     =$(SRC:.c=.o)
OPT_FLG =-O2

all: $(OBJ)

include ../../Makefile.inc
include ../../vars.mk

clean:
	rm -f *.o
