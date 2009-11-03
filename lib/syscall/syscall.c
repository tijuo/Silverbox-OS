#include <oslib.h>

void _sysenter( int callNum, struct SysCallArgs *args )
{
  asm __volatile__ ("sysenter\n" :: "a"(callNum), "b"(args->arg1),
                    "c"(args->arg2), "d"(args->arg3), "S"(args->arg4),
                    "D"(args->arg5));
}