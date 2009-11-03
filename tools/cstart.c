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

mutex_t print_lock=0;

void print( char *str )
{
  char volatile *vidmem = (char *)(0xB8000 + 160 * 4);
  static int i=0;

  while(mutex_lock(&print_lock))
    __yield();

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

  mutex_unlock(&print_lock);
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
