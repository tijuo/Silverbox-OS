#include <oslib.h>
#include <os/mutex.h>
#include <os/signal.h>

extern int main(int, char **);

void __dummy_sig_handler(int signal, int code);

struct StartArgs
{
  int argc;
  char **argv;
};

void printC( char c );
void printN( char *str, int n );
void print( char *str );
void printInt( int n );
void printHex( int n );
void _start( struct StartArgs *start_args );

mutex_t print_lock=0;

void printC( char c )
{
  char volatile *vidmem = (char *)(0xB8000 + 160 * 4);
  static int i=0;

  while(mutex_lock(&print_lock))
    __yield();

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

void __dummy_sig_handler(int signal, int code)
{
}

void _start( struct StartArgs *start_args )
{
  int retVal;

  /* Init stuff here */

//  _init_out_queue();
//  _init_pct_table();

  set_signal_handler( &__dummy_sig_handler );
  __set_sig_handler( _default_sig_handler );

  if( start_args != NULL )
    retVal = main(start_args->argc, start_args->argv);
  else
    retVal = main(0, NULL);

  /* Cleanup stuff here */

  __exit( retVal );
}
