#include <os/syscalls.h>

extern "C" int **start_ctors, **end_ctors, **start_dtors, **end_dtors;

extern int main(int, char**);

extern "C" void _start( void )
{
  int retCode;
  int *startPtr;

  startPtr = (int *)&start_ctors;

  while( startPtr != (int *)&end_ctors )
  {
    (*(void (*)())(*startPtr))();
    startPtr++;
  }

  retCode = main(0, NULL);

  startPtr = (int *)&start_dtors;

  while( startPtr != (int *)&end_dtors )
  {
    (*(void (*)())(*startPtr))();
    startPtr++;
  }

  sys_exit( retCode );
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
