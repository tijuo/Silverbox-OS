#include <stdio.h>

int feof(FILE *fp)
{
  if( fp == NULL )
    return 0;
  else 
    return (fp->eof == 1);
}
