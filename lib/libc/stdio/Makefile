include ../../../prefix.inc

SRC	=clearerr.c do_printf.c feof.c ferror.c fflush.c fgetc.c fprintf.c \
        fputc.c fputs.c fseek.c ftell.c getchar.c perror.c \
        printf.c putchar.c puts.c rewind.c setvbuf.c sprintf.c \
        stdio_init.c vfprintf.c vprintf.c vsprintf.c fopen.c fread.c \
	fclose.c

OBJ     =$(SRC:.c=.o)
OPT_FLG =-O2

all: $(OBJ)

include ../../../Makefile.inc
include ../../../vars.mk

clean:
	rm -f *.o
