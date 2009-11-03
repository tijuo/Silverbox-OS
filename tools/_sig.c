#include <os/signal.h>

void set_signal_handler( void (*handler)(int, int) )
{
  __default_sig_handler = handler;
}
