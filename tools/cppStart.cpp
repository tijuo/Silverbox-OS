#include <oslib.h>

extern "C" int **start_ctors, **end_ctors, **start_dtors, **end_dtors;

extern int main(int, char**);
extern "C" void _init_out_queue(void);

struct StartArgs
{
  int argc;
  char **argv;
};

extern "C" void print( char *str )
{
  char volatile *vidmem = (char *)(0xB8000 + 160 * 15);
  static int i=0;

  while(*str)
  {
    if( *str == '\n' )
    {
      i += 160 - (i % 160);
      str++;
      continue;
    }
    vidmem[i++] = *str++;
    vidmem[i++] = 7;
  }
}

extern "C" void _start( struct StartArgs *start_args )
{
  __map((void *)0xB8000, (void *)0xB8000, 8);

  int retCode;
  int *startPtr;

  startPtr = (int *)&start_ctors;

  while( startPtr != (int *)&end_ctors )
  {
    (*(void (*)())(*startPtr))();
    startPtr++;
  }

  _init_out_queue();

  retCode = main(start_args->argc, start_args->argv);

  startPtr = (int *)&start_dtors;

  while( startPtr != (int *)&end_dtors )
  {
    (*(void (*)())(*startPtr))();
    startPtr++;
  }

  __exit( retCode );
}

/*
%include "asm/asm.inc"

[section .text]
[global start]
[extern main]
[extern start_ctors]
[extern end_ctors]
[extern start_dtors]
[extern end_dtors]
[extern exit]

start:

        mov     ebx, start_ctors
.nextCtor
        cmp     ebx, end_ctors
        je      .endCtorCall
        call    [ebx]
        add     ebx, 4
        jmp     .nextCtor

.endCtorCall

        call    main
        push    eax

        mov     ebx, start_dtors
.nextDtor
        cmp     ebx, end_dtors
        je      .endDtorCall
        call    [ebx]
        add     ebx, 4
        jmp     .nextDtor
.endDtorCall
;       Tell kernel to exit here

        call exit
*/
