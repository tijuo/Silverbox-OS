#include <oslib.h>
#include <string.h>
#include <os/dev_interface.h>
#include <os/services.h>
#include <os/console.h>

int main( void )
{
  char letter[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
  int i=0;

//  mapMem( (void *)0xB8000, (void *)0xB8000, 8, 0 );
//  __map( (void *)0xB8000, (void *)0xB8000, 8 );

  __sleep( 4000 );

  printMsg("Hi!\rHello! I'm talking to the video server!\nTesting...");
  print("Should've written something");

  while( true )
  {
    printMsg("\f\n");

    printMsg( letter );

    if( letter[i] == 'z' )
      letter[i] = 'a';
    else
      letter[i] = letter[i] + 1;
    i++;
    i %= 80;
  }
  return 0;
//  return printMsg( "Hi!\rHello! I'm talking to the video server!\fTesting..." );
}
