#ifdef DEBUG

#include <kernel/debug.h>
#include <kernel/memory.h>
#include <kernel/mm.h>
#include <kernel/thread.h>
#include <kernel/io.h>
#include <stdarg.h>
#include <kernel/lowlevel.h>

#define SCREEN_HEIGHT   25
#define SCREEN_WIDTH    80

bool useLowMem;
bool badAssertHlt;
dword upper1, lower1, upper2, lower2;

void kprintAt( const char *str, int x, int y );
void kprintf( const char *str, ... );
void resetScroll( void );
void scrollUp( void );
int scrollDown( void );
void doNewLine( int *x, int *y );
char *kitoa(int value, char *str, int base, int unSigned);
//char *klltoa(long long int value, char *str, int base, int unSigned);
void _putChar( char c, int x, int y, unsigned char attrib );
void putChar( char c, int x, int y );
void dump_regs( const tcb_t *thread, const ExecutionState *state, int intNum, int errorCode);
void dump_state( const ExecutionState *state, int intNum, int errorCode);
void dump_stack( addr_t, addr_t );
static const char *_digits="0123456789abcdefghijklmnopqrstuvwxyz";

unsigned int sx=0, sy=1;

#define com1 0x3f8
#define com2 0x2f8

#define combase com1

/** Sets up the serial ports for printing debug messages
    (and possibly receiving commands).
*/

void startTimeStamp(void)
{
  rdtsc(&upper1, &lower1);
}

void stopTimeStamp(void)
{
  rdtsc(&upper2, &lower2);
}

unsigned int getTimeDifference(void)
{
  if( upper1 == upper2 )
    return (unsigned)(lower2 - lower1);
  else if( upper2 == upper1 + 1 )
  {
    if( lower2 < lower1 )
        return (0xFFFFFFFFu - lower1) + lower2;
  }
  return 0xFFFFFFFFu;
}

void init_serial(void)
{
    outByte(combase + 3, inByte(combase + 3) | 0x80u); // Set DLAB for
                                                      // DLLB access
    outByte(combase, 0x03u);                           /* 38400 baud */
    outByte(combase + 1, 0u); // Disable interrupts
    outByte(combase + 3, inByte(combase + 3) & 0x7fu); // Clear DLAB
    outByte(combase + 3, 3u); // 8-N-1
}

int getDebugChar(void)
{
    while (!(inByte(combase + 5) & 0x01u));
    return inByte(combase);
}

void putDebugChar(int ch)
{
    outByte(0xE9, (byte)ch); // Use E9 hack to output characters
    while (!(inByte(combase + 5) & 0x20u));
    outByte(combase, (byte) ch);
/*
if( ch == '\r' )
{
 sx=0;
 sy++;
 if( sy >= SCREEN_HEIGHT )
   scrollDown();
}
else if( ch == '\t' )
{
 sx +=8;
}
else
{
  putChar(ch,sx,sy);
  sx++;
}

if( sx >= SCREEN_WIDTH )
{
  sx=0;
  sy++;
  if( sy >= SCREEN_HEIGHT )
    scrollDown();
}
*/
}

char *kitoa(int value, char *str, int base, int unSigned)
{
  unsigned int num = (unsigned int)value;
  size_t str_end=0;
  int negative = !unSigned && (value & 0x80000000);

  if(negative)
    num = ~num + 1;

  if( !str )
    return NULL;
  else if( base >= 2 && base <= 36 )
  {
    if(negative)
      str[str_end++] = '-';

    do
    {
      unsigned int quot = base * (num / base);

      str[str_end++] = _digits[num - quot];
      num = quot / base;
    } while( num );
  }

  str[str_end] = '\0';

  if( str_end )
  {
    str_end--;

    for( size_t i=0; i < str_end; i++, str_end-- )
    {
      char tmp = str[i];
      str[i] = str[str_end];
      str[str_end] = tmp;
    }
  }
  return str;
}

/*
char *klltoa(long long int value, char *str, int base, int unSigned)
{
  unsigned long long int num = (unsigned long long int)value;
  size_t str_end=0;
  int negative = !unSigned && (value & 0x8000000000000000ull);

  if(negative)
    num = ~num + 1;

  if( !str )
    return NULL;
  else if( base >= 2 && base <= 36 )
  {
    if(negative)
      str[str_end++] = '-';

    do
    {
      unsigned long long int quot = base * (num / base);

      str[str_end++] = _digits[num - quot];
      num = quot / base;
    } while( num );
  }

  str[str_end] = '\0';

  if( str_end )
  {
    str_end--;

    for( size_t i=0; i < str_end; i++, str_end-- )
    {
      char tmp = str[i];
      str[i] = str[str_end];
      str[str_end] = tmp;
    }
  }
  return str;
}
*/

/*
char *klltoa(long long int value, char *str, int base)
{
  unsigned long long int num = (unsigned long long int)value;
  size_t str_end=0;

  if( !str )
    return NULL;
  else if( base >= 2 && base <= 36 )
  {
    if( base == 10 && value < 0 )
    {
      str[str_end++] = '-';
    }
    do
    {
      unsigned long long int quot = base * (num / base);

      str[str_end++] = _digits[num - quot];
      num = quot / base;
    } while( num );
  }

  str[str_end] = '\0';

  if( str_end )
  {
    str_end--;

    for( size_t i=0; i < str_end; i++, str_end-- )
    {
      char tmp = str[i];
      str[i] = str[str_end];
      str[str_end] = tmp;
    }
  }
  return str;
}
*/
void initVideo( void )
{
  useLowMem = false;
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

/** Sets the flag to use conventional memory for printing instead
    of virtual memory.
*/
void setVideoLowMem( bool value )
{
  useLowMem = value;
}

/// Sets the flag to halt the system on a failed assertion

void setBadAssertHlt( bool value )
{
  badAssertHlt = value;
}

void resetScroll( void )
{
//  word address=0;

  outByte( 0x3D4u, 0x0Cu );
  outByte( 0x3D5u, 0 );
  outByte( 0x3D4u, 0x0Du );
  outByte( 0x3D5u, 0 );
}

void scrollUp( void )
{
  byte high, low;
  word address;

  outByte( 0x3D4u, 0x0Cu );
  high = inByte( 0x3D5u );
  outByte( 0x3D4u, 0x0Du );
  low = inByte( 0x3D5u );
  address = (high << 8 | low );

  if( address == 0 )
    address = 0x3fc0u;
  else
    address -= 0x50u;

  outByte( 0x3D4u, 0x0Cu );
  outByte( 0x3D5u, address >> 8 );
  outByte( 0x3D4u, 0x0Du );
  outByte( 0x3D5u, address & 0xFFu );
}

int scrollDown( void )
{
  byte high, low;
  word address;
  int ret = 0;

  outByte( 0x3D4u, 0x0Cu );
  high = inByte( 0x3D5u );
  outByte( 0x3D4u, 0x0Du );
  low = inByte( 0x3D5u );

  address = (high << 8 | low );
  address += 0x50u;

  if( address > 0x3e80u )
  {
    address = 0;
    ret = 1;
  }

  outByte( 0x3D4u, 0x0Cu );
  outByte( 0x3D5u, address >> 8 );
  outByte( 0x3D4u, 0x0Du );
  outByte( 0x3D5u, address & 0xFFu );

  return ret;
}


void kprintAt( const char *str, int x, int y )
{
  volatile word *vidmem = (volatile word *)VIDMEM_START;
  size_t offset = 80 * y + x, i=0;

  if( !useLowMem )
    vidmem += PHYSMEM_START;

  while(str[i])
  {
    vidmem[offset] = (vidmem[offset] & 0xFF00u) | (byte)str[i];
    offset++;
    i++;
  }
}


void printAssertMsg(const char *exp, const char *file, const char *func, int line)
{
  kprintf("\n<'%s' %s: %d> assert(%s) failed\n", file, func, line, exp);

  if( currentThread )
    dump_regs((tcb_t *)currentThread, &currentThread->execState, 0, 0);

  if( badAssertHlt )
  {
    kprintf("\nSystem halted.");

    disableInt();
    asm("hlt");
  }
}

/** Increments a counter that keeps track of the number of times that
    the scheduler was called.
*/

CALL_COUNTER(incSchedCount)
void incSchedCount( void )
{
  volatile char *vidmem = ( volatile char * ) ( VIDMEM_START );
  char digits[12];

  if( !useLowMem )
    vidmem += PHYSMEM_START;

  vidmem[(SCREEN_WIDTH - 1) * 2]++;
  vidmem[(SCREEN_WIDTH - 1) * 2 + 1] = 0x78;

  assert( currentThread != NULL );

  if( currentThread )
  {
    kprintAt("            ", 2, 0);
    kprintAt( kitoa((int)currentThread->tid, digits, 10, 0), 2, 0 );
    kprintAt( kitoa((int)currentThread->priority, digits, 10, 0), 5, 0 );
    kprintAt( "        ", 30, 0 );
    kprintAt( kitoa((int)currentThread->quantaLeft, digits, 10, 0), 8, 0 );
    kprintAt( kitoa(*(int *)&currentThread->rootPageMap, digits, 16, 0), 30, 0 );
  }
}

/** Increments a counter that keeps track of the number of times that
    the timer interrupt was called.
*/

void incTimerCount( void )
{
  volatile char *vidmem = ( volatile char * ) ( VIDMEM_START );
  char digits[12];

  if( !useLowMem )
    vidmem += PHYSMEM_START;

  vidmem[0]++;
  vidmem[1] = 0x72;

  kprintAt( kitoa((int)currentThread->quantaLeft, digits, 10, 0), 8, 0 );
}

/// Blanks the screen

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


/** A simplified stdio printf() for debugging use
    @param str The formatted string to print with format specifiers
    @param ... The arguments to the specifiers
*/

void kprintf( const char *str, ... )
{
  int longCounter=0;
  int unSigned=0;
  int percent = 0;
  va_list args;
  char digits[12];
  size_t i;

  va_start(args, str);

  for(i=0; str[i]; i++ )
  {
    if( percent )
    {
      switch( str[i] )
      {
        case 'l':
          longCounter++;
          break;
        case '%':
          putDebugChar('%');
          longCounter = 0;
          unSigned = 0;
          percent = 0;
          break;
        case 'c':
          putDebugChar((char)va_arg(args,char));
          longCounter = 0;
          unSigned = 0;
          percent = 0;
          break;
        case 'p':
        case 'x':
        case 'X':
          if(longCounter >= 2)
           {
             long long int arg = va_arg(args, long long int);

             if(arg >> 32)
               kprintf(kitoa((int)(arg >> 32), digits, 16, 0));

             kprintf(kitoa((int)(arg & 0xFFFFFFFF), digits, 16, 0));
           }
           else
             kprintf(kitoa(va_arg(args, int), digits, 16, 0));

          longCounter = 0;
          unSigned = 0;
          percent = 0;
          break;
        case 's':
          kprintf(va_arg(args, char *));
          percent=0;
          longCounter=0;
          unSigned = 0;
          break;
        case 'u':
          unSigned = 1;
          break;
        case 'd':
        case 'i':
/*
          if(longCounter >= 2)
            kprintf(klltoa(va_arg(args, long long int), digits, 10));
          else
*/
          kprintf(kitoa(va_arg(args, int), digits, 10, unSigned));

          percent = 0;
          longCounter = 0;
          unSigned = 0;
          break;
        default:
          break;
      }
    }
    else
    {
      switch( str[i] )
      {
        case '%':
          percent = 1;
          break;

        case '\n':
          //doNewLine( x, y );
          putDebugChar('\r');
          putDebugChar('\n');
          break;
/*
        case '\r':
          *x = 0;
          break;
*/
        default:
          putDebugChar( str[i] );

        //  if( ++(*x) == SCREEN_WIDTH )
        //    doNewLine( x, y );
          break;
      }
    }
  }
  va_end(args);
}

void dump_state( const ExecutionState *execState, int intNum, int errorCode )
{
  if( execState == NULL )
  {
    kprintf("Unable to show execution state.\n");
    return;
  }

  if( intNum == 0x40 )
  {
    kprintf("Syscall");
  }
  else if( intNum < IRQ0 )
  {
    kprintf("Exception %d", intNum);
  }
  else if( intNum >= IRQ0 && intNum <= IRQ15 )
  {
    kprintf("IRQ%d", intNum - IRQ0);
  }
  else
  {
    kprintf("Software Interrupt %d", intNum);
  }

  kprintf(" @ EIP: 0x%x", execState->eip);

  kprintf( "\nEAX: 0x%x EBX: 0x%x ECX: 0x%x EDX: 0x%x", execState->eax, execState->ebx, execState->ecx, execState->edx );
  kprintf( "\nESI: 0x%x EDI: 0x%x EBP: 0x%x", execState->esi, execState->edi, execState->ebp );
  kprintf( "\nCS: 0x%x", execState->cs );

  if( intNum == 14 )
  {
    kprintf(" CR2: 0x%x", getCR2());
  }

  kprintf( " error code: 0x%x\n", errorCode );

  kprintf("EFLAGS: 0x%x ", execState->eflags);

  if( execState->cs == UCODE )
  {
    kprintf("ESP: 0x%x User SS: 0x%x\n", execState->userEsp, execState->userSS);
  }
}

void dump_stack( addr_t stackFramePtr, addr_t addrSpace )
{
  kprintf("\n\nStack Trace:\n<Stack Frame>: [Return-EIP] args*\n");

  while( stackFramePtr )
  {
    kprintf("<0x%x>:", stackFramePtr);

    if( isReadable( stackFramePtr + 4, addrSpace ) )
    {
      kprintf(" [0x%x]", *(dword *)(stackFramePtr + 4));
    }
    else
    {
      kprintf(" [???]");
    }

    for( int i=2; i < 8; i++ )
    {
      if( isReadable( stackFramePtr + 4 * i, addrSpace) )
      {
        kprintf(" 0x%x", *(dword *)(stackFramePtr + 4 * i));
      }
      else
      {
        break;
      }
    }

    kprintf("\n");

    if( !isReadable(*(dword *)stackFramePtr, addrSpace) )
    {
      kprintf("<0x%x (invalid)>:\n", *(dword *)stackFramePtr);
      break;
    }
    else
    {
      stackFramePtr = *(dword *)stackFramePtr;
    }
  }
}

/// Prints useful debugging information about the current thread

void dump_regs( const tcb_t *thread, const ExecutionState *execState, int intNum, int errorCode )
{
  addr_t stackFramePtr;

  kprintf( "Thread: 0x%x (TID: %d) ", thread, thread->tid);

/*
  if( ((addr_t)thread - (addr_t)tcbTable) % sizeof *thread == 0 &&
      thread->tid >= INITIAL_TID )
  {
    kprintf("TID: %d ", thread->tid);
  }
  else
  {
    kprintf("(invalid thread address) ");
  }
*/
//  if( (unsigned int)(thread + 1) == tssEsp0 ) // User thread
//  {
    execState = (ExecutionState *)&thread->execState;
//  }

  dump_state(execState, intNum, errorCode);

  if( !execState )
  {
    kprintf("\n");
  }

  kprintf( "Thread CR3: 0x%x Current CR3: 0x%x\n", *(unsigned *)&thread->rootPageMap, getCR3() );

  if( !execState )
  {
    __asm__("mov %%ebp, %0\n" : "=m"(stackFramePtr));

    if( !isReadable(*(dword *)stackFramePtr, thread->rootPageMap) )
    {
      kprintf("Unable to dump the stack\n");
      return;
    }

    stackFramePtr = *(addr_t *)stackFramePtr;
  }
  else
  {
    stackFramePtr = (addr_t)execState->ebp;
  }

  dump_stack(stackFramePtr, thread->rootPageMap);
}

#endif /* DEBUG */

void abort(void);

void abort(void)
{
  kprintf("Debug Error: abort() has been called.");
  __asm__("hlt");
}
