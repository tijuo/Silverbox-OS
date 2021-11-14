include ../../../Makefile.inc

ROOT_DIR	:=../../..

.PHONY:	all tests clean

SRC	=clearerr.c do_printf.c fclose.c feof.c ferror.c fflush.c \
	fgetc.c fopen.c fprintf.c fputc.c fputs.c fread.c fseek.c \
	ftell.c getchar.c os_stub.c perror.c printf.c putchar.c \
	puts.c rewind.c setbuf.c setvbuf.c sprintf.c stdio_init.c \
	vfprintf.c vprintf.c vsprintf.c
OBJ	=$(SRC:.c=.o)

all: $(OBJ)

tests:

clean:
	rm -f *.o

include ../../../Makefile_post.inc
