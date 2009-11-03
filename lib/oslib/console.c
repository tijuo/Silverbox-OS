#include <oslib.h>
#include <string.h>
#include <os/keys.h>
#include <os/dev_interface.h>
#include <os/services.h>
#include <os/console.h>

tid_t video_srv = NULL_TID;
tid_t kb_srv = NULL_TID;

int printChar( char c );
int printMsg( char *msg );
int printStrN( char *str, size_t len );
char kbConvertRawChar( unsigned char c );
char kbGetRawChar( void );
char kbGetChar( void );

/* XXX: Code is needed here to control scrolling, colors, etc */

int printChar( char c )
{
  if( video_srv == NULL_TID )
    video_srv = lookupName("video", strlen("video"));

  return deviceWrite( video_srv, 0, 0, 1, 1, &c );
}

int printStrN( char *str, size_t len )
{
  if( video_srv == NULL_TID )
    video_srv = lookupName("video", strlen("video"));

  return deviceWrite( video_srv, 0, 0, len, 1, str );
}

int printMsg( char *msg )
{
  if( video_srv == NULL_TID )
    video_srv = lookupName("video", strlen("video"));

  return deviceWrite( video_srv, 0, 0, strlen(msg), 1, msg );
}

char kbConvertRawChar( unsigned char c )
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
  else if( numlk && (c >= VK_KP_0 && c <= VK_KP_9) )
    return '0' + (c-VK_0);
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
    case VK_KP_MINUS:
      return '-';
    case VK_KP_PLUS:
      return '+';
    case VK_KP_SLASH:
      return '/';
    case VK_KP_PERIOD:
      return '.';
    case VK_KP_ASTR:
      return '*';
    case VK_KP_ENTER:
      return '\n';
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

char kbGetRawChar( void )
{
  char kbChar;

  if( kb_srv == NULL_TID )
    kb_srv = lookupName("keyboard", strlen("keyboard"));

  if( deviceRead( kb_srv, 0, 0, 1, 1, &kbChar ) < 0 )
    return '\0';
  else
    return kbChar;
}

char kbGetChar( void )
{
  char c = '\0';
  
  do
  {
    c = kbConvertRawChar(kbGetRawChar());
  } while( c == '\0' );

  return c;
}
