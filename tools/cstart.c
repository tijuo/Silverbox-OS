#include <oslib.h>
#include <os/mutex.h>
#include <os/syscalls.h>

extern int main(int, char **);

/*
struct StartArgs
{
  int argc;
  char **argv;
};
*/
void printC( char c );
void printN( char *str, int n );
void print( char *str );
void printInt( int n );
void printHex( int n );
void _start( /*struct StartArgs *start_args*/ );

mutex_t print_lock=0;

void printC( char c )
{
  char volatile *vidmem = (char *)(0xB8000 + 160 * 4);
  static int i=0;

  while(mutex_lock(&print_lock))
    sys_wait(0);

  if( c == '\n' )
    i += 160 - (i % 160);
  else
  {
    vidmem[i++] = c;
    vidmem[i++] = 7;
  }

  mutex_unlock(&print_lock);
}

void printN( char *str, int n )
{
  for( int i=0; i < n; i++, str++ )
    printC(*str);
}

void print( char *str )
{
  for(; *str; str++ )
    printC(*str);
}

void printInt( int n )
{
  print(toIntString(n));
}

void printHex( int n )
{
  print(toHexString(n));
}

void _start( /*struct StartArgs *start_args*/ )
{
  int retVal;

  /* Init stuff here */

    retVal = main(0, NULL);

  /* Cleanup stuff here */

  sys_exit( retVal );
}
