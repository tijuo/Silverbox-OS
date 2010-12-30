#include <oslib.h>
#include <string.h>
#include <os/dev_interface.h>
#include <os/services.h>
#include <os/keys.h>
#include <os/console.h>

//#define LINE_READ

//char buffer[1024];
/*
int printMsg( char *msg )
{
  return send( 5, DEVICE_WRITE, msg, strlen(msg), -1 );
}

int printChar( char c )
{
  return send( 5, DEVICE_WRITE, &c, 1, -1 );
}
*/

char convert_raw_char( unsigned char c )
{
  static int alt=0, caps=0, shift=0, numlk=0;

  char num_shift[10] = { ')', '!', '@', '#', '$', '%', '^', '&', '*','(' };

  if( (c & VK_BREAK) == VK_BREAK )
  {
    if( (c & 0x7F) == VK_RSHIFT || (c & 0x7F) == VK_LSHIFT )
      shift = 0;
    return '\0';
  }
  else if( c == VK_CAPSLK )
    caps ^= 1;
  else if( c == VK_NUMLK )
    numlk ^= 1;

  c &= 0x7F;

  if( c >= VK_0 && c <= VK_9 )
    return (shift ? num_shift[c-VK_0] : '0' + (c - VK_0));
  else if( c >= VK_A && c <= VK_Z )
    return ((!shift && !caps) || (shift && caps) ? 'a' + (c - VK_A) : 'A' + (c-VK_A));
  else if( c == VK_RSHIFT || c == VK_LSHIFT )
    shift = 1;

  switch( c )
  {
    case VK_ENTER:
      return '\n';
    case VK_BSPACE:
      return '\b';
    case VK_TAB:
      return '\t';
    case VK_SPACE:
      return ' ';
    case VK_MINUS:
      return (shift ? '_' : '-');
    case VK_EQUAL:
      return (shift ? '+' : '=');
    case VK_BQUOTE:
      return (shift ? '~' : '`');
    case VK_COMMA:
      return (shift ? '<' : ',');
    case VK_PERIOD:
      return (shift ? '>' : '.');
    case VK_SLASH:
      return (shift ? '?' : '/');
    case VK_BSLASH:
      return (shift ? '|' : '\\');
    case VK_RBRACK:
      return (shift ? '}' : ']');
    case VK_LBRACK:
      return (shift ? '{' : '[');
    case VK_QUOTE:
      return (shift ? '"' : '\'');
    case VK_SEMICOL:
      return (shift ? ':' : ';');
    default:
      return '\0';
  }
  return '\0';
}

int main( void )
{
  char kbChar;

  __sleep( 1500 );
//  __map(0xB8000,0xB8000, 8);
//  mapMem( (void *)0xB8000, (void *)0xB8000, 8, 0 );

  printMsg("\n\n\n\nReady.\n");
  print("\n\n\n\n\n\n\ngo\n");

  while( true )
  {
/*
    i = 0;

    #ifdef LINE_READ
    while( (buffer[i++] = getChar()) != '\n' );
    #else
    buffer[i++] = getChar();
    #endif

    buffer[i] = '\0';

    printMsg( buffer );
*/
    kbChar = kbGetChar();
/*
    if( kbChar == '\b' )
    {
      printChar('\b');
      printChar(' ');
      printChar('\b');
    }
    else
*/
    if( kbChar != '\0' )
      printChar( kbChar );
  }

//  return printMsg( "Hi!\rHello! I'm talking to the video server!\fTesting..." );
}
