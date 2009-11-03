#!/bin/bash

# Compiles and links a C++ file into an executable

source `dirname $0`/variables.sh

CPP_STUB="cppStart"
CPP_RT="cpprt"
OSLIB0="oslib0"
CPPFLAGS="-O0 -m32 -Wall -fno-builtin -nostdlib -nostartfiles -fno-exceptions -fno-rtti"
CFLAGS="-O0 -m32 -Wall -std=gnu99 -fno-builtin -nostdlib -ffreestanding"
#Note: Final slash after directory is important!
INCLUDES="$SB_PREFIX/include/ $SB_PREFIX/drivers/include/"
EXIT_ON_WARN=1
TOOLS="$SB_PREFIX/tools"
LIBS="$SB_PREFIX/lib/"

if [ $# -lt 2 ]; then
  echo "usage: $0 infile-1 [ infile-2 ... infile-n ] output"
  exit
fi

if ! [ -f "$TOOLS/$CPP_STUB.o" ]; then
  if ! [ -f "$TOOLS/$CPP_STUB.cpp" ]; then
    echo "Missing $CPP_STUB.cpp"
    exit 1
  fi
  g++ -c `for i in $INCLUDES; do echo -n "-I $i "; done` $CPPFLAGS $TOOLS/$CPP_STUB.cpp -o $TOOLS/$CPP_STUB.o
fi

if ! [ -f "$TOOLS/$CPP_RT.o" ]; then
  if ! [ -f "$TOOLS/$CPP_RT.cpp" ]; then
    echo "Missing $CPP_RT.cpp"
    exit 1
  fi

  g++ -c `for i in $INCLUDES; do echo -n "-I $i "; done` $CPPFLAGS $TOOLS/$CPP_RT.cpp -o $TOOLS/$CPP_RT.o
fi

if ! [ -f "$TOOLS/$OSLIB0.o" ]; then
  if ! [ -f "$TOOLS/$OSLIB0.c" ]; then
    echo "Missing $OSLIB0.c"
    exit 1
  fi
  gcc -c `for i in $INCLUDES; do echo -n "-I $i "; done` $CFLAGS $TOOLS/$OSLIB0.c -o $TOOLS/$OSLIB0.o
fi

files="`echo $* | cut --complement -d ' ' -f $#`"
output="`echo $* | cut -d ' ' -f $#`"
obj="`echo $files | sed 's/\.c\(pp\)\{0,1\}/\.o/g' -`"
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

g++ -c `for i in $INCLUDES; do echo -n "-I $i "; done` $CPPFLAGS $files
ld -melf_i386 -T $SB_PREFIX/cppLink.ld $TOOLS/{$CPP_STUB,$CPP_RT,$OSLIB0}.o $obj -\( -lc -los -\) -o $output
