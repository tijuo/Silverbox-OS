#!/bin/bash

# Compiles and links a C file into an executable

source `dirname $0`/set_variables.sh

C_STUB="cstart"
CFLAGS="-O0 -Wall -m32 -std=gnu11 -fno-builtin -nostdlib -ffreestanding -nostartfiles -nodefaultlibs"
ASFLAGS="-O0 -f elf"

#Note: Final slash after directory is important!
INCLUDES="$SB_PREFIX/include/ $SB_PREFIX/drivers/include/"
EXIT_ON_WARN=1
TOOLS="$SB_PREFIX/tools"
LIBS="$SB_PREFIX/lib/libc/ $SB_PREFIX/lib/libos/"
LIBGCC=`i686-elf-gcc --print-libgcc-file-name`
GCCLIBS=`dirname $LIBGCC`
LDFLAGS="-melf_i386 --exclude-libs ALL -T $SB_PREFIX/cLink.ld --static -L$GCCLIBS -( -lc -los -lgcc -)"

if [ $# -lt 2 ]; then
  echo "usage: $0 infile-1 [ infile-2 ... infile-n ] output"
  exit
fi

#if ! [ -f "$TOOLS/$C_STUB.o" ]; then
  if ! [ -f "$TOOLS/$C_STUB.c" ]; then
    echo "Missing $C_STUB.c"
    exit 1
  fi
  i686-elf-gcc -c `for i in $INCLUDES; do echo -n "-I $i "; done` $CFLAGS $TOOLS/$C_STUB.c -o $TOOLS/$C_STUB.o
#fi

files="`echo $* | cut --complement -d ' ' -f $#`"
output="`echo $* | cut -d ' ' -f $#`"
obj="`echo $files | sed 's/\.c/\.o/g' -`"
suffix="`echo $output | cut -d '.' -f 2`"

for name in $files; do
  if [ $name == $output ]; then
    echo "Warning: Input file is the same as output file."

    if [ $EXIT_ON_WARN ]; then exit 1; fi
  fi

  if [ $suffix == "c" ] || [ $suffix == "cpp" ]; then
    echo "Warning: Output file may be a source file."

    if [ $EXIT_ON_WARN ]; then exit 1; fi
  fi
done

i686-elf-gcc -c `for i in $INCLUDES; do echo -n "-I $i "; done` $CFLAGS $files
i686-elf-ld $obj $TOOLS/$C_STUB.o `for l in $LIBS; do echo -n "-L $l "; done` $LDFLAGS -o $output
