#ifdef DEBUG

#include <kernel/debug.h>
#include <kernel/memory.h>
#include <kernel/mm.h>
#include <kernel/thread.h>
#include <os/io.h>
#include <oslib.h>
#include <stdarg.h>

#define abs(a)	(a < 0 ? -a : a)

#define SCREEN_HEIGHT   25
#define SCREEN_WIDTH    80

void kprintAt( const char *str, int x, int y );
void kprintf( const char *str, ... );
//void resetScroll( void );
//void scrollUp( void );
//int scrollDown( void );
//void doNewLine( int *x, int *y );
char *kitoa(int value, char *str, int base);

#define com1 0x3f8
#define com2 0x2f8

#define combase com1

void init_serial(void)
{
    outByte(combase + 3, inByte(combase + 3) | 0x80); // Set DLAB for
                                                      // DLLB access
    outByte(combase, 0x03);                           /* 38400 baud */
    outByte(combase + 1, 0); // Disable interrupts
    outByte(combase + 3, inByte(combase + 3) & 0x7f); // Clear DLAB
    outByte(combase + 3, 3); // 8-N-1
}

int getDebugChar(void)
{
    while (!(inByte(combase + 5) & 0x01));
    return inByte(combase);
}

void putDebugChar(int ch)
{
    while (!(inByte(combase + 5) & 0x20));
    outByte(combase, (char) ch);
}

char *kitoa(int value, char *str, int base)
{
  char _buf[33], *buf=_buf;
  unsigned digit;
  char *begin = str;
  unsigned *_val = (unsigned *)&value;

  if( str == NULL || base > 36 || base < 2 )
    return NULL;
  else if( value == 0 )
  {
    *str = '0';
    *(str+1) = '\0';
    return str;
  }

  *buf++ = '\0';

  if( base == 10 && value < 0)
    *str++ = '-';

  while( (base == 10 ? (unsigned)value : *_val) )
  {
    digit = (base == 10 ? (unsigned)abs(value) % base : *_val % base);
    *buf++ = (digit >= 10 ? (digit - 10) + 'A' : digit + '0');

    if( base == 10 )
      value /= base;
    else
      *_val /= base;
  }

  while( *--buf != '\0' )
    *str++ = *buf;

  *str = '\0';
  return begin;
}

void initVideo( void )
{
  useLowMem = true;
  badAssertHlt = true;//false; // true;
}
/*
char toIntString(int num)
{
  static char digits[12];

  return kitoa(num, digits, 10);
}

char toHexString(unsigned num)
{
  static char digits[9];

  return kitoa(num, digits, 16);
}

char *__toIntString(int num, int sign)
{
  unsigned i = 0, j = 0;
  const char iChars[] = { '0','1','2','3','4','5','6','7','8','9' };
  unsigned d_num[17];
  static char s_num[11];

  s_num[0] = 0;

  if(num == 0){
    s_num[0] = '0';
    s_num[1] = '\0';
    return s_num;
  }

  if( sign && num < 0 )
  {
    s_num[j++] = '-';
    num = -num;
  }

  if( sign )
  {
    for( ; num > 0; i++, num /= 10 )
      d_num[i] = num % 10;
  }
  else
  {
    for( ; (unsigned)num > 0; i++, num /= 10 )
      d_num[i] = (unsigned)num % 10;
  }

  i--;

  while((i + 1) > 0)
    s_num[j++] = iChars[d_num[i--]];

  s_num[j] = '\0';
  return s_num;
}

char *_toIntString(int num)
{
  return __toIntString(num, 1);
}

char *__toHexString(unsigned int num, int lower)
{
  unsigned i = 0, j = 0;
  
  char const uHexChars[] = { '0','1','2','3','4','5','6','7','8','9', 'A', 'B',
                            'C', 'D', 'E', 'F' };
  char const lHexChars[] = { '0','1','2','3','4','5','6','7','8','9', 'a', 'b',
                             'c', 'd', 'e', 'f' }; 
  char const *hexChars;
                           
  static char s_num[9];
  unsigned h_num[9];

  if( lower )
    hexChars = lHexChars;
  else
    hexChars = uHexChars;

  s_num[0] = 0;

  if(num == 0)
  {
    s_num[0] = '0';
    s_num[1] = '\0';
    return s_num;
  }

  for( ; num > 0; i++, num /= 16 )
    h_num[i] = num % 16;

  i--;

  while( (i + 1) > 0 )
    s_num[j++] = hexChars[h_num[i--]];

  s_num[j] = '\0';

  return s_num;
}

char *_toHexString(unsigned int num)
{
  return __toHexString(num, 0);
}
*/
void setVideoLowMem( bool value )
{
  useLowMem = value;
}

void setBadAssertHlt( bool value )
{
  badAssertHlt = value;
}
/*
void resetScroll( void )
{
  volatile word address=0;

  outByte( 0x3D4, 0x0C );
  outByte( 0x3D5, address >> 8 );
  outByte( 0x3D4, 0x0D );
  outByte( 0x3D5, address & 0x0F );
}

void scrollUp( void )
{
  volatile byte high, low;
  volatile word address;

  outByte( 0x3D4, 0x0C );
  high = inByte( 0x3D5 );
  outByte( 0x3D4, 0x0D );
  low = inByte( 0x3D5 );
  address = (high << 8 | low );

  if( address == 0 )
    address = 0x3fc0;
  else
    address -= 0x50;

  outByte( 0x3D4, 0x0C );
  outByte( 0x3D5, address >> 8 );
  outByte( 0x3D4, 0x0D );
  outByte( 0x3D5, address & 0xFF );
}

int scrollDown( void )
{
  volatile byte high, low;
  volatile word address;
  int ret = 0;

  outByte( 0x3D4, 0x0C );
  high = inByte( 0x3D5 );
  outByte( 0x3D4, 0x0D );
  low = inByte( 0x3D5 );

  address = (high << 8 | low );
  address += 0x50;

  if( address > 0x3e80 )
  {
    address = 0;
    ret = 1;
  }

  outByte( 0x3D4, 0x0C );
  outByte( 0x3D5, address >> 8 );
  outByte( 0x3D4, 0x0D );
  outByte( 0x3D5, address );

  return ret;
}
*/

void kprintAt( const char *str, int x, int y )
{
  volatile char *vidmem = (volatile char *)( VIDMEM_START + 2 * (x + 80 * y));

  if( !useLowMem )
    vidmem += PHYSMEM_START;

  while(*str)
    *vidmem++ = *str++, vidmem++;
}


void printAssertMsg(const char *exp, const char *file, const char *func, int line)
{
  kprintf("\n<'%s' %s: %d> assert(%s) failed\n", file, func, line, exp);

  if( badAssertHlt )
  {
    kprintf("\nSystem halted.");

    disableInt();
    asm("hlt");
  }
}

void incSchedCount( void )
{
  volatile char *vidmem = ( volatile char * ) ( VIDMEM_START );
  char digits[12];

  if( !useLowMem )
    vidmem += PHYSMEM_START;

  vidmem[(SCREEN_WIDTH - 1) * 2]++;
  vidmem[(SCREEN_WIDTH - 1) * 2 + 1] = 0x78;

  assert( currentThread != NULL );

  if( currentThread != NULL )
  {
    kprintAt("     ", 2, 0);
    kprintAt( kitoa((int)GET_TID(currentThread), digits, 10), 2, 0 );
    kprintAt( "        ", 30, 0 );
    kprintAt( kitoa((int)currentThread->addrSpace, digits, 16), 30, 0 );
  }
}

void incTimerCount( void )
{
  volatile char *vidmem = ( volatile char * ) ( VIDMEM_START );

  if( !useLowMem )
    vidmem += PHYSMEM_START;

  vidmem[0]++;
  vidmem[1] = 0x72;
}

void clearScreen( void )
{
  volatile char *vidmem = ( volatile char * ) ( VIDMEM_START );

  if( !useLowMem )
    vidmem += PHYSMEM_START;

  memset( (char *)vidmem, 0, SCREEN_HEIGHT * SCREEN_WIDTH * 2 );

  for( int i=0; i < SCREEN_WIDTH; i++ )
  {
    vidmem[i << 1] = ' ';
    vidmem[(i << 1)+1] = 0x70;
  }
  //resetScroll();
}

/*
void doNewLine( int *x, int *y )
{
  (*y)++;
  *x = 0;

  if( *y > 24 )
  {
    if( scrollDown() )
      *y = 0;
  }
}
*/

/*
void _putChar( char c, int x, int y, unsigned char attrib )
{
  volatile char *vidmem = (char *)(VIDMEM_START);

  if( !useLowMem )
    vidmem += PHYSMEM_START;

  vidmem[ 2 * (y * SCREEN_WIDTH + x) ] = c;
  vidmem[ 2 * (y * SCREEN_WIDTH + x) + 1 ] = attrib;
}

void putChar( char c, int x, int y )
{
  _putChar( c, x, y, 7 );
}
*/
void kprintf( const char *str, ... )
{
//  unsigned i;
  char percent = 0;
  va_list args;
 // int start = *y * SCREEN_WIDTH + *x;
  char digits[12];

  va_start(args, str);

  for(; *str; str++ )
  {
    if( percent )
    {
      switch( *str )
      {
        case '%':
          putDebugChar('%');//putChar('%', *x, *y);
          break;
        case 'c':
          putDebugChar((char)va_arg(args,char));//putChar((char)va_arg(args, char));
          break;
        case 'p':
        case 'x':
        case 'X':
          kprintf(kitoa(va_arg(args, unsigned int), digits, 16));
          break;
        case 's':
          kprintf(va_arg(args, char *));
          break;
        case 'u':
        case 'd':
        case 'i':
           kprintf(kitoa(va_arg(args, unsigned int), digits, 10));
          break;
        default:
          break;
      }
      percent = 0;
    }
    else
    {
      switch( *str )
      {
        case '%':
          percent = 1;
          break;

        case '\n':
          //doNewLine( x, y );
          putDebugChar('\r');
         // putDebugChar('\n');
          break;
/*
        case '\r':
          *x = 0;
          break;
*/
        default:
          putDebugChar( *str );

        //  if( ++(*x) == SCREEN_WIDTH )
        //    doNewLine( x, y );
          break;
      }
    }
  }
  va_end(args);
}
#endif /* DEBUG */
